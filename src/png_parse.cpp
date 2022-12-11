#include "png_parse.hpp"

#include <tinf.h>
#include <cstring>
#include <cinttypes>
#include <cstdlib>
#include <cmath>

namespace img_parse
{
  uint32_t read_u32(png_parse_context_s& ctx, uint64_t offset)
  {
    //PNG byte order to host
    return (uint32_t)((uint32_t)(ctx.data[ctx.offset + offset] << 24) + (uint32_t)(ctx.data[ctx.offset + offset + 1] << 16) + (uint32_t)(ctx.data[ctx.offset + offset + 2] << 8) + (uint32_t)(ctx.data[ctx.offset + offset + 3] << 0)); 
  }

  uint8_t read_u8(png_parse_context_s& ctx, uint64_t offset)
  {
    return *(ctx.data + ctx.offset + offset);
  }

  bool check_next_chunk(png_parse_context_s& ctx, chunk_data_s& cd)
  {
    //first must be the file header
    if(ctx.offset < 8) return false;

    //if remaining bytes not enough for the smallest chunk possible, it's invalid
    uint32_t remaining = ctx.size - ctx.offset;
    if(remaining < 12) return false;    

    //if the chunk would be bigger than the remaining bytes, it's invalid
    cd.len = read_u32(ctx, 0);
    if(remaining < (cd.len + 4 + 4)) return false;

    //check the integrity of the chunk with crc32
    cd.crc32 = read_u32(ctx, cd.len + 4 + 4);
    uint32_t crc_calc = tinf_crc32(ctx.data + ctx.offset + 4, cd.len + 4);
    if(cd.crc32 != crc_calc) return false;

    //read the type
    cd.type = read_u32(ctx, 4);
    return true;
  }

  bool parse_ihdr(png_parse_context_s& ctx)
  {
    chunk_data_s cd;
    if(!check_next_chunk(ctx, cd)) return false; //does crc check too
    if(cd.type != chunk_type_ihdr) return false;
    if(cd.len != 13) return false; //IHDR must be 13 bytes long
    ctx.offset += 4 + 4; //len, type

    ctx.hdr.width = read_u32(ctx, 0);
    ctx.hdr.height = read_u32(ctx, 4);
    ctx.hdr.bit_depth = read_u8(ctx, 8);
    ctx.hdr.color_type = read_u8(ctx, 9);
    ctx.hdr.compression_method = read_u8(ctx, 10);
    ctx.hdr.filter_method = read_u8(ctx, 11);
    ctx.hdr.interlace_method = read_u8(ctx, 12);

    ctx.offset += cd.len + 4; //data + crc32
    return true;
  }

  uint8_t paeth_predictor(uint8_t a, uint8_t b, uint8_t c)
  {
    int p = a + b - c;
    int pa = abs(p - a);
    int pb = abs(p - b);
    int pc = abs(p - c);
    int pr;
    if(pa <= pb && pa <= pc)
        pr = a;
    else if(pb <= pc)
        pr = b;
    else
        pr = c;
    return pr;
  }

  void unfilter_scanline(png_parse_context_s& ctx)
  {
    uint8_t filter_method = *(ctx.inflated_data + (ctx.stride + 1) * ctx.scanline_index);
    switch (filter_method)
    {
    case filter_method_none:
    {
      memcpy(ctx.unfiltered_data + ctx.stride * ctx.scanline_index, 
             ctx.inflated_data + (ctx.stride + 1) * ctx.scanline_index + 1,
             ctx.stride);
      break;
    }
    case filter_method_sub:
    {
      for(uint32_t i = 0; i < ctx.stride; i++)
      {
        ctx.unfiltered_data[ctx.stride * ctx.scanline_index + i] = ctx.inflated_data[(ctx.stride + 1) * ctx.scanline_index + 1 + i];
        if(i < ctx.pixel_size)
          continue;
        else
          ctx.unfiltered_data[ctx.stride * ctx.scanline_index + i] += ctx.unfiltered_data[ctx.stride * ctx.scanline_index  + i - ctx.pixel_size];
      }
      break;
    }
    case filter_method_up:
    {
      for(uint32_t i = 0; i < ctx.stride; i++)
      {
        ctx.unfiltered_data[ctx.stride * ctx.scanline_index + i] = ctx.inflated_data[(ctx.stride + 1) * ctx.scanline_index + 1 + i];
        if(ctx.scanline_index == 0)
          return;
        else
          ctx.unfiltered_data[ctx.stride * ctx.scanline_index + i] += ctx.unfiltered_data[ctx.stride * (ctx.scanline_index - 1) + i];
      }
      break;
    }
    case filter_method_avg:
    {
      for(uint32_t i = 0; i < ctx.stride; i++)
      {
        ctx.unfiltered_data[ctx.stride * ctx.scanline_index + i] = ctx.inflated_data[(ctx.stride + 1) * ctx.scanline_index + 1 + i];

        uint8_t r_a = 0;
        uint8_t r_b = 0;
        if(i >= ctx.pixel_size)
          r_a = ctx.unfiltered_data[ctx.stride * ctx.scanline_index  + i - ctx.pixel_size];
        if(ctx.scanline_index > 0)
          r_b = ctx.unfiltered_data[ctx.stride * (ctx.scanline_index - 1) + i];

        ctx.unfiltered_data[ctx.stride * ctx.scanline_index + i] += (r_a + r_b) / 2;
      }
      break;
    }
    case filter_method_paeth:
    {
      for(uint32_t i = 0; i < ctx.stride; i++)
      {
        ctx.unfiltered_data[ctx.stride * ctx.scanline_index + i] = ctx.inflated_data[(ctx.stride + 1) * ctx.scanline_index + 1 + i];

        uint8_t r_a = 0;
        uint8_t r_b = 0;
        uint8_t r_c = 0;
        if(i >= ctx.pixel_size)
          r_a = ctx.unfiltered_data[ctx.stride * ctx.scanline_index  + i - ctx.pixel_size];
        if(ctx.scanline_index > 0)
          r_b = ctx.unfiltered_data[ctx.stride * (ctx.scanline_index - 1) + i];
        if(i >= ctx.pixel_size && ctx.scanline_index > 0)
          r_c = ctx.unfiltered_data[ctx.stride * (ctx.scanline_index - 1) + i - ctx.pixel_size];

        ctx.unfiltered_data[ctx.stride * ctx.scanline_index + i] += paeth_predictor(r_a, r_b, r_c);
      }
      break;
    }
    default:
      return;
      break;
    }
  }

  bool parse_idat(png_parse_context_s& ctx)
  {
    chunk_data_s cd;
    if(!check_next_chunk(ctx, cd)) return false; //does crc check too
    if(cd.type != chunk_type_idat) return false;
    if(cd.len < 7) return false;
    ctx.offset += 4 + 4; //len, type

    //allocate buffer for inflated data, can be calculated based on the ihdr data
    ctx.inflated_data = (uint8_t*)calloc(ctx.hdr.height * (1 + ctx.hdr.width * ctx.pixel_size), 1);
    if(ctx.inflated_data == NULL) return false;
    ctx.inflated_size = ctx.hdr.height * (1 + ctx.hdr.width * ctx.pixel_size);

    //read compressed stuff and uncompress it
    // uint8_t compression_method = *(ctx.data + ctx.offset);
    // uint8_t additional_flags = *(ctx.data + ctx.offset + 1);
    int ret = tinf_uncompress(ctx.inflated_data, &ctx.inflated_size, ctx.data + ctx.offset + 2, cd.len);
    if(ret != TINF_OK)
    {
      free(ctx.inflated_data);
      ctx.inflated_data = NULL;
      ctx.inflated_size = 0;
      return false;
    }

    //check the inflated size if it matches the ihdr based size
    if(ctx.inflated_size != ctx.hdr.height * (1 + ctx.hdr.width * ctx.pixel_size))
    {
      free(ctx.inflated_data);
      ctx.inflated_data = NULL;
      ctx.inflated_size = 0;
      return false;
    }

    //calculate how much byte represents one scanline
    ctx.stride = ctx.hdr.width * ctx.pixel_size;

    //allocated buffor for the image representation
    ctx.unfiltered_data = (uint8_t*)calloc(ctx.hdr.height * ctx.stride, 1);
    if(ctx.unfiltered_data == NULL)
    {
      free(ctx.inflated_data);
      ctx.inflated_data = NULL;
      ctx.inflated_size = 0;
      return false;
    }
    ctx.unfiltered_size = ctx.hdr.height * ctx.stride;

    //unfilter the inflated data
    for(ctx.scanline_index = 0; ctx.scanline_index < ctx.hdr.height; ctx.scanline_index++)
      unfilter_scanline(ctx);

    //we don't need the infalted data buffer any more, deallocating it
    free(ctx.inflated_data);
    ctx.inflated_data = NULL;
    ctx.inflated_size = 0;

    //adjusting parsing offset
    ctx.offset += cd.len + 4; //data + crc32
    return true;
  }

  bool parse_iend(png_parse_context_s& ctx)
  {
    chunk_data_s cd;
    if(!check_next_chunk(ctx, cd)) return false; //does crc check too
    if(cd.type != chunk_type_iend) return false;
    if(cd.len != 0) return false; //IHDR must be 13 bytes long
    ctx.parsed = true;
    ctx.offset = ctx.size - 1;
    return true;
  }

  bool parse_plte(png_parse_context_s& ctx)
  {
    return true;
  }

  bool parse_next_chunk(png_parse_context_s& ctx)
  {
    if(ctx.parsed) return false;
    chunk_data_s cd;
    if(!check_next_chunk(ctx, cd)) return false; //does crc check too

    switch (cd.type)
    {
    case chunk_type_ihdr:
    {
      if(!parse_ihdr(ctx)) return false;
      break;
    }
    case chunk_type_idat:
    {
      if(!parse_idat(ctx)) return false;
      break;
    }
    case chunk_type_iend:
    {
      if(!parse_iend(ctx)) return false;
      break;
    }
    default:
      ctx.offset += cd.len + 4 + 4 + 4; //skip entire chunk (len, type, data, crc32)
      break;
    }

    return true;
  }

  bool check_header(png_parse_context_s& ctx)
  {
    if(ctx.size < 8) return false;
    if(ctx.offset != 0) return false;

    if(ctx.data[0] != 0x89) return false;

    //ASCII 'PNG'
    if(ctx.data[1] != 0x50) return false;
    if(ctx.data[2] != 0x4E) return false;
    if(ctx.data[3] != 0x47) return false;

    //DOS style line ending
    if(ctx.data[4] != 0x0D) return false;
    if(ctx.data[5] != 0x0A) return false;

    //DOS style EOF
    if(ctx.data[6] != 0x1A) return false;

    //UNIX style line ending
    if(ctx.data[7] != 0x0A) return false;

    ctx.offset += 8;
    return true;
  }

  bool init(png_parse_context_s& ctx, uint8_t* data, uint32_t len)
  {
    memset(&ctx, 0, sizeof(ctx));
    ctx.data = (uint8_t*)calloc(len, 1);
    if(ctx.data == NULL) return false;
    ctx.size = len;
    memcpy(ctx.data, data, ctx.size);
    return true;
  }

  void deinit(png_parse_context_s& ctx)
  {
    if(ctx.data) free(ctx.data);
    if(ctx.inflated_data) free(ctx.inflated_data);
    if(ctx.unfiltered_data) free(ctx.unfiltered_data);
    memset(&ctx, 0, sizeof(ctx));
  }

  bool parse(png_parse_context_s& ctx)
  {
    //check the png header and the first ihdr
    if(!check_header(ctx)) return false;
    if(!parse_ihdr(ctx)) return false;

    //only supporting 32bit rgba or 24bit rgb pixel data
    if(ctx.hdr.bit_depth != 8) return false; 

    if(ctx.hdr.color_type == 6)
      ctx.pixel_size = 4;
    else if(ctx.hdr.color_type == 2)
      ctx.pixel_size = 3;
    else
      return false;

    //parse the following chunks until the first iend chunk is not found
    while (!ctx.parsed)
      if(!parse_next_chunk(ctx)) return false;

    //deallocate raw input buffer
    free(ctx.data);
    ctx.data = NULL;
    ctx.size = 0;
    return true;
  }
}
