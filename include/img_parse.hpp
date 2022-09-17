#include <cstring>
#include <cinttypes>
#include <cstdlib>
#include <cmath>

#define INDEX_STREAM_ALLOCATION_BLOCK 512

namespace img_parse
{
  typedef enum block_type_e
  {
    block_type_image_descriptor = 0x2C,
    block_type_trailer = 0x3B,
    block_type_extension_introducer = 0x21,
  } block_type_e;

  typedef enum block_label_e
  {
    block_label_graphic_control = 0xF9,
    block_label_comment = 0xFE,
    block_label_plain_text = 0x01,
    block_label_application = 0xFF,
  } block_label_e;

  typedef struct color_s
  {
    uint8_t r;
    uint8_t g;
    uint8_t b;
  } color_s;

  typedef struct lsd_fields_s
  {
    uint8_t global_color_table_size :3;
    uint8_t sort_flag :1;
    uint8_t color_resolution :3;
    uint8_t global_color_table_flag :1;
  } lsd_fields_s;

  typedef struct
  {
    uint8_t local_color_table_size :3;
    uint8_t reserved :2;
    uint8_t sort_flag :1;
    uint8_t interlace_flag :1;
    uint8_t local_color_table_flag :1;
  } id_fields_s;
  
  typedef struct logical_screen_descriptor_s
  {
    uint16_t width;
    uint16_t height;
    lsd_fields_s fields;
    uint8_t  background_color_index;
    uint8_t  pixel_aspect_ratio;
  } logical_screen_descriptor_s;

  typedef struct image_descriptor_s
  {
    uint16_t left_position;
    uint16_t top_position;
    uint16_t width;
    uint16_t height;
    id_fields_s fields;
  } image_descriptor_s;

  typedef struct code_table_entry_s
  {
    uint8_t* string;
    uint8_t string_size;
  } code_table_entry_s;

  typedef struct code_table_s
  {
    code_table_entry_s* entries;
    uint32_t entries_count;
    uint16_t cc;
    uint16_t eoi;
  } code_table_s;

  typedef struct image_s
  {
    image_descriptor_s id;

    //local color table
    color_s* lct;
    uint32_t lct_size;

    //lzw parse
    uint8_t code_size;

    uint8_t* lzw;
    uint32_t lzw_size;
    uint32_t lzw_offset_byte;
    uint8_t  lzw_offset_bit;

    code_table_s code_table;

    uint8_t* index_stream;
    uint32_t index_stream_size;
    uint32_t index_stream_offset;

    uint32_t last_code;
    uint32_t code;

    //output data in RGB
    color_s* output;  
    uint32_t output_size;

  } image_s;  

  typedef struct gif_parse_context_s
  {
    bool parsed;

    //raw input data to parse
    uint8_t* input;
    size_t input_size;
    uint32_t offset;

    logical_screen_descriptor_s lsd;

    //global color table
    color_s *gct;
    uint32_t gct_size;

    image_s* images;
    uint32_t images_size;

  } gif_parse_context_s;

  bool parse_gct(gif_parse_context_s& ctx)
  {
    if(ctx.parsed) return false;
    if(!ctx.lsd.fields.global_color_table_flag) return true; //if there is no gct we are done
    if(ctx.offset != 6 + 7) return false; //should follow header and lsd

    //calc the gct size and check the input size
    uint32_t gct_size = (0x01 << (ctx.lsd.fields.global_color_table_size + 1)) * 3;
    if(ctx.input_size < (6 + 7 + gct_size)) return false;

    //allocate memory for gct and copy it
    ctx.gct = (color_s*)calloc(gct_size, 1);
    if(ctx.gct == NULL) return false;
    ctx.gct_size = gct_size / 3;
    memcpy(ctx.gct, ctx.input + ctx.offset, gct_size);

    ctx.offset += gct_size;
    return true;
  }

  bool parse_lsd(gif_parse_context_s& ctx)
  {
    if(ctx.parsed) return false;
    if(ctx.offset != 6) return false; //should follow header
    if(ctx.input_size < 6 + 6) return false;

    memcpy(&ctx.lsd.width, ctx.input + ctx.offset, sizeof(ctx.lsd.width));
    memcpy(&ctx.lsd.height, ctx.input + ctx.offset + 2, sizeof(ctx.lsd.height));
    memcpy(&ctx.lsd.fields, ctx.input + ctx.offset + 4, sizeof(ctx.lsd.fields));
    ctx.lsd.background_color_index = ctx.input[ctx.offset + 5];
    ctx.lsd.pixel_aspect_ratio = ctx.input[ctx.offset + 6];

    ctx.offset += 7;
    return true;
  }

  bool check_header(gif_parse_context_s& ctx)
  {
    if(ctx.input_size < 6) return false;
    if(ctx.offset != 0) return false;

    //ASCII 'GIF'
    if(ctx.input[0] != 0x47) return false;
    if(ctx.input[1] != 0x49) return false;
    if(ctx.input[2] != 0x46) return false;

    //version '89a'
    if(ctx.input[3] != 0x38) return false;
    if(ctx.input[4] != 0x39 && ctx.input[4] != 0x37) return false;
    if(ctx.input[5] != 0x61) return false;

    ctx.offset += 6;
    return true;
  }
  
  bool add_code_table_entry(image_s* image, uint16_t code, void* string_pt, uint32_t string_size)
  {
    if(!image) return false;
    if(image->code_size < 2) return false;
    if(!image->code_table.entries) return false;
    if(code >= 0x01 << (image->code_size + 1)) return false; //over indexing

    //alloc and copy
    image->code_table.entries[code].string = (uint8_t*)calloc(0x01, string_size);
    if(!image->code_table.entries[code].string) return false;
    memcpy(image->code_table.entries[code].string, string_pt, string_size);
    image->code_table.entries_count++;
    image->code_table.entries[code].string_size = string_size;

    if(image->code_table.entries_count == 0x01 << (image->code_size + 1)) //code size bump time baby
    {
      image->code_size++;
      image->code_table.entries = (code_table_entry_s*)realloc(image->code_table.entries, sizeof(code_table_entry_s) * 0x01 << (image->code_size + 1));
      if(!image->code_table.entries) return false;
    }

    return true;
  }

  bool init_code_table(image_s* image)
  {
    if(!image) return false;
    if(image->code_size < 2) return false; //min allowed code size

    //allocate mem for code table                     
    image->code_table.entries = (code_table_entry_s*)calloc(0x01 << (image->code_size + 1), sizeof(code_table_entry_s));
    image->code_table.entries_count = 0;
    if(!image->code_table.entries) return false;

    //add trivial codes
    for(uint16_t code = 0; code < (0x01 << image->code_size) + 2; code++)
      add_code_table_entry(image, code, &code, 0x01); //+2 means it adds cc and eoi too

    //control codes for convenience
    image->code_table.cc =  (0x01 << image->code_size);
    image->code_table.eoi = (0x01 << image->code_size) + 1;

    return true;
  }

  bool parse_lct(gif_parse_context_s& ctx)
  {
    if(ctx.parsed) return false;
    if(ctx.images == NULL) return false;
    if(ctx.images_size == 0) return false;

    image_s* image_pt = ctx.images + (ctx.images_size - 1);
    if(!image_pt->id.fields.local_color_table_flag) return true;

    //calc the gct size and check the input size
    uint32_t lct_size = (0x01 << (image_pt->id.fields.local_color_table_size + 1)) * 3;
    if(ctx.input_size < (ctx.offset + lct_size)) return false;

    //allocate memory for gct and copy it
    image_pt->lct = (color_s*)calloc(lct_size, 1);
    if(image_pt->lct == NULL) return false;
    image_pt->lct_size = lct_size / 3;
    memcpy(image_pt->lct, ctx.input + ctx.offset, lct_size);

    ctx.offset += lct_size;
    return true;
  }

  bool read_code(image_s* image)
  {
    if(image == NULL) return false;

    //save the last code
    image->last_code = image->code;

    //read two bytes
    memcpy(&(image->code), image->lzw + image->lzw_offset_byte, sizeof(image->code));

    //remove the unnecessary bits before the code
    image->code = image->code >> image->lzw_offset_bit; 

    //maks the unnecessary bits after the code
    uint16_t mask = 0;
    for(uint16_t i = 0; i < (image->code_size + 1); i++)
      mask |= 0x01 << i;
    image->code &= mask;

    //adjust the offsets
    image->lzw_offset_bit += image->code_size + 1;
    if(image->lzw_offset_bit >= 8)
    {
      image->lzw_offset_byte += image->lzw_offset_bit / 8;
      image->lzw_offset_bit = image->lzw_offset_bit % 8;
    }

    return true;
  }

  bool is_in_code_table(image_s* image, uint16_t code)
  {
    if(code > (0x01 << (image->code_size + 1))) return false;
    return image->code_table.entries[code].string_size != 0 ? true : false;
  }

  bool output_index(image_s* image, bool first_only = false, bool last = false)
  {
    if(!image) return false;
    uint32_t code = last ? image->last_code : image->code;
    if(!is_in_code_table(image, code)) return false;
    uint32_t copy_size = first_only ? 1 : image->code_table.entries[code].string_size;

    //realloc if necessary
    if(image->index_stream_offset + copy_size > image->index_stream_size)
    {
      image->index_stream = (uint8_t*)realloc(image->index_stream, image->index_stream_size + INDEX_STREAM_ALLOCATION_BLOCK);
      if(!image->index_stream) return false;
      image->index_stream_size = image->index_stream_size + INDEX_STREAM_ALLOCATION_BLOCK;
    }

    //append string to the index stream
    memcpy(image->index_stream + image->index_stream_offset, image->code_table.entries[code].string, copy_size);
    image->index_stream_offset += copy_size;

    return true;
  }

  bool parse_image(gif_parse_context_s& ctx)
  {
    if(ctx.parsed) return false;
    if(*(ctx.input + ctx.offset) != block_type_image_descriptor) return false;
    if(ctx.input_size - ctx.offset < 10) return false; //at least the id

    //allocate mem for the new image data
    if(ctx.images == NULL)
    {
      ctx.images = (image_s*)calloc(1, sizeof(image_s));
      if(ctx.images == NULL) return false;
      ctx.images_size = 1;
    }
    else
    {
      ctx.images = (image_s*)realloc(ctx.images, sizeof(image_s) * (ctx.images_size + 1));
      if(ctx.images == NULL)
      {
        ctx.images_size = 0;
        return false;
      }
      ctx.images_size++;
    }

    image_s* image_pt = ctx.images + (ctx.images_size - 1);

    //parse image descriptor of the image
    memcpy(&image_pt->id.left_position, ctx.input + ctx.offset + 1, 2);
    memcpy(&image_pt->id.top_position, ctx.input + ctx.offset + 1 + 2, 2);
    memcpy(&image_pt->id.width, ctx.input + ctx.offset + 1 + 4, 2);
    memcpy(&image_pt->id.height, ctx.input + ctx.offset + 1 + 6, 2);
    memcpy(&image_pt->id.fields, ctx.input + ctx.offset + 1 + 8, 1);

    //don't supported functions
    if(image_pt->id.fields.interlace_flag) return false;
    if(image_pt->id.height != ctx.lsd.height) return false;
    if(image_pt->id.width != ctx.lsd.width) return false;

    //parse local color table
    ctx.offset += 1 + 9;
    if(!parse_lct(ctx)) return false;

    //minimum code size in bits
    image_pt->code_size = *(ctx.input + ctx.offset); 
    if(image_pt->code_size < 2) image_pt->code_size == 2; //min allowed code size

    //read and concatenate data sub-blocks into lzw buffer
    uint64_t offset = ctx.offset + 1;
    while(*(ctx.input + offset) != 0x00) //block terminator
    {
      uint8_t sub_block_size = *(ctx.input + offset);
      if((ctx.input_size - offset) < sub_block_size) return false;      
      image_pt->lzw = (uint8_t*)realloc(image_pt->lzw, image_pt->lzw_size + sub_block_size);
      if(image_pt->lzw == NULL) return false;
      memcpy(image_pt->lzw + image_pt->lzw_size, ctx.input + offset + 1, sub_block_size);
      image_pt->lzw_size += sub_block_size;
      image_pt->lzw_offset_byte = 0;
      image_pt->lzw_offset_bit = 0;
      offset += sub_block_size + 1;
    }
    ctx.offset = offset + 1;

    //parse lzw
    init_code_table(image_pt);
    
    //first code should be cc
    if(!read_code(image_pt)) return false;
    if(image_pt->code != image_pt->code_table.cc) return false;

    //second code should be in the table
    if(!read_code(image_pt)) return false;
    output_index(image_pt);

    for(uint i = 0;; i++)
    {
      if(!read_code(image_pt)) return false;
      if(image_pt->code == image_pt->code_table.cc)
      {
        //todo, save starting code_size and reinit with that
        init_code_table(image_pt);
        break;
      }
      if(image_pt->code == image_pt->code_table.eoi) break;
      if(image_pt->lzw_offset_byte >= image_pt->lzw_size) break; //protection against over reading
      if(is_in_code_table(image_pt, image_pt->code))
      {
        output_index(image_pt);
        uint8_t c = *(image_pt->code_table.entries[image_pt->code].string); //first index of the current code
        //overcopy for fewer allocation
        add_code_table_entry(image_pt, image_pt->code_table.entries_count, image_pt->code_table.entries[image_pt->last_code].string, image_pt->code_table.entries[image_pt->last_code].string_size + 1); 
        image_pt->code_table.entries[image_pt->code_table.entries_count - 1].string[image_pt->code_table.entries[image_pt->code_table.entries_count - 1].string_size - 1] = c;
      }
      else
      {
        output_index(image_pt, false, true); //all indexes of the last code
        output_index(image_pt, true, true); //first index of the last code
        uint32_t last_string_size = image_pt->code_table.entries[image_pt->last_code].string_size;
        add_code_table_entry(image_pt, image_pt->code, image_pt->index_stream + image_pt->index_stream_offset - last_string_size - 1, last_string_size + 1);
      }
    }

    //todo: deinit code table and entries in it

    if(image_pt->index_stream_offset != image_pt->id.height * image_pt->id.width) return false;

    //create RGBA based on index stream
    color_s* color_table = image_pt->id.fields.local_color_table_flag ? image_pt->lct : ctx.gct;
    uint32_t color_table_size = image_pt->id.fields.local_color_table_flag ? image_pt->lct_size : ctx.gct_size;
    image_pt->output = (color_s*)calloc(image_pt->index_stream_offset, sizeof(color_s));
    if(!image_pt->output) return false;
    image_pt->output_size = image_pt->index_stream_offset;
    for(uint32_t i = 0; i < image_pt->index_stream_offset; i++)
      memcpy(&image_pt->output[i], &color_table[image_pt->index_stream[i]], sizeof(color_s));
    
    return true;
  }

  bool parse_extension(gif_parse_context_s& ctx)
  {
    if(ctx.parsed) return false;
    if(ctx.input_size - ctx.offset < 4) return false;
    if(*(ctx.input + ctx.offset) != block_type_extension_introducer) return false;

    uint8_t label = *(ctx.input + ctx.offset + 1);
    uint8_t block_size = *(ctx.input + ctx.offset + 2);
    if((ctx.input_size - ctx.offset) < (3 + block_size)) return false;

    uint32_t offset = ctx.offset + 3 + block_size;    
    while(*(ctx.input + offset) != 0x00) //block terminator
    {
      if((ctx.input_size - offset) < *(ctx.input + offset)) return false;
      offset += *(ctx.input + offset) + 1;
    }

    ctx.offset = offset + 1;
    return true;
  }

  bool parse_next_block(gif_parse_context_s& ctx)
  {
    if(ctx.parsed) return false;

    uint8_t block_label = *(ctx.input + ctx.offset);

    switch (block_label)
    {
    case block_type_image_descriptor:
    {
      parse_image(ctx);
      break;
    }
    case block_type_trailer:
    {
      ctx.parsed = true;
      break;
    }
    case block_type_extension_introducer:
    {
      //skips all extension blocks
      parse_extension(ctx);
      break;
    }
    default:
    //handle unhandled blocks :)
      break;
    }

    return true;
  }

  bool init(gif_parse_context_s& ctx, uint8_t* input, size_t input_size)
  {
    //init the context struct
    memset(&ctx, 0, sizeof(ctx));

    //dynamically allocate mem for input and store the input data
    ctx.input = (uint8_t*)calloc(input_size, 1);
    if(ctx.input == NULL) return false;
    ctx.input_size = input_size;
    memcpy(ctx.input, input, ctx.input_size);

    return true;
  }

  bool parse(gif_parse_context_s& ctx)
  {
    if(ctx.parsed) return false;
    if(!check_header(ctx)) return false;
    if(!parse_lsd(ctx)) return false;
    if(!parse_gct(ctx)) return false;

    while(!ctx.parsed)
      parse_next_block(ctx);

    free(ctx.input);
    ctx.input = NULL;
    ctx.input_size = 0;
    return true;
  }

  void deinit(gif_parse_context_s& ctx)
  {
    //deallocate all dynamically allocated memory and zero the entire struct
    if(ctx.input) free(ctx.input);
    if(ctx.gct) free(ctx.gct);
    memset(&ctx, 0, sizeof(ctx));
  }
  
} // namespace img_parse
