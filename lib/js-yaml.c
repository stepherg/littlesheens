#include <duktape.h>
#include <stdio.h>
#include <string.h>
#include "yaml.h"

// Helper: Convert libyaml node to Duktape object
static duk_idx_t yaml_document_to_js(duk_context *ctx, yaml_document_t *doc, yaml_node_t *node)
{
   const char *str = NULL;

   if (!node)
   {
      duk_push_null(ctx);
      return duk_get_top_index(ctx);
   }

   switch (node->type)
   {
   case YAML_SCALAR_NODE:
      str = (const char *)node->data.scalar.value;

      // test for boolean
      if ((strcmp(str, "true") == 0) ||
            (strcmp(str, "True") == 0) || 
            (strcmp(str, "TRUE") == 0))
         duk_push_boolean(ctx, 1); // Push true
      else if ((strcmp(str, "false") == 0)  ||
            (strcmp(str, "False") == 0) ||
            (strcmp(str, "FALSE") == 0))
         duk_push_boolean(ctx, 0); 
      else
         duk_push_string(ctx, str);
      break;
   case YAML_SEQUENCE_NODE:
   {
      duk_push_array(ctx);
      yaml_node_item_t *item = node->data.sequence.items.start;
      duk_idx_t idx = 0;
      while (item != node->data.sequence.items.top)
      {
         yaml_node_t *child = yaml_document_get_node(doc, *item);
         yaml_document_to_js(ctx, doc, child);
         duk_put_prop_index(ctx, -2, idx++);
         item++;
      }
      break;
   }
   case YAML_MAPPING_NODE:
   {
      duk_push_object(ctx);
      yaml_node_pair_t *pair = node->data.mapping.pairs.start;
      while (pair != node->data.mapping.pairs.top)
      {
         yaml_node_t *key_node = yaml_document_get_node(doc, pair->key);
         yaml_node_t *value_node = yaml_document_get_node(doc, pair->value);
         if (key_node->type == YAML_SCALAR_NODE)
         {
            yaml_document_to_js(ctx, doc, value_node);
            duk_put_prop_string(ctx, -2, (const char *)key_node->data.scalar.value);
         }
         pair++;
      }
      break;
   }
   default:
      duk_push_null(ctx);
   }
   return duk_get_top_index(ctx);
}

// Helper: Duktape object to libyaml emitter
static void js_to_yaml_emitter(duk_context *ctx, duk_idx_t idx, yaml_emitter_t *emitter)
{
   yaml_event_t event;

   if (duk_is_string(ctx, idx))
   {
      const char *value = duk_get_string(ctx, idx);
      yaml_scalar_event_initialize(&event, NULL, (yaml_char_t *)"tag:yaml.org,2002:str", (yaml_char_t *)value, strlen(value), 1, 1, YAML_PLAIN_SCALAR_STYLE);
      yaml_emitter_emit(emitter, &event);
   }
   else if (duk_is_number(ctx, idx))
   {
      char buffer[32];
      snprintf(buffer, sizeof(buffer), "%g", duk_get_number(ctx, idx));
      yaml_scalar_event_initialize(&event, NULL, (yaml_char_t *)"tag:yaml.org,2002:float", (yaml_char_t *)buffer, strlen(buffer), 1, 1, YAML_PLAIN_SCALAR_STYLE);
      yaml_emitter_emit(emitter, &event);
   }
   else if (duk_is_boolean(ctx, idx))
   {
      const char *value = duk_get_boolean(ctx, idx) ? "true" : "false";
      yaml_scalar_event_initialize(&event, NULL, (yaml_char_t *)"tag:yaml.org,2002:bool", (yaml_char_t *)value, strlen(value), 1, 1, YAML_PLAIN_SCALAR_STYLE);
      yaml_emitter_emit(emitter, &event);
   }
   else if (duk_is_array(ctx, idx))
   {
      yaml_sequence_start_event_initialize(&event, NULL, (yaml_char_t *)"tag:yaml.org,2002:seq", 1, YAML_ANY_SEQUENCE_STYLE);
      yaml_emitter_emit(emitter, &event);
      duk_enum(ctx, idx, DUK_ENUM_ARRAY_INDICES_ONLY);
      while (duk_next(ctx, -1, 1))
      {
         js_to_yaml_emitter(ctx, -1, emitter);
         duk_pop_2(ctx);
      }
      duk_pop(ctx);
      yaml_sequence_end_event_initialize(&event);
      yaml_emitter_emit(emitter, &event);
   }
   else if (duk_is_object(ctx, idx))
   {
      yaml_mapping_start_event_initialize(&event, NULL, (yaml_char_t *)"tag:yaml.org,2002:map", 1, YAML_ANY_MAPPING_STYLE);
      yaml_emitter_emit(emitter, &event);
      duk_enum(ctx, idx, DUK_ENUM_OWN_PROPERTIES_ONLY);
      while (duk_next(ctx, -1, 1))
      {
         const char *key = duk_get_string(ctx, -2);
         yaml_scalar_event_initialize(&event, NULL, (yaml_char_t *)"tag:yaml.org,2002:str", (yaml_char_t *)key, strlen(key), 1, 1, YAML_PLAIN_SCALAR_STYLE);
         yaml_emitter_emit(emitter, &event);
         js_to_yaml_emitter(ctx, -1, emitter);
         duk_pop_2(ctx);
      }
      duk_pop(ctx);
      yaml_mapping_end_event_initialize(&event);
      yaml_emitter_emit(emitter, &event);
   }
   else
   {
      yaml_scalar_event_initialize(&event, NULL, (yaml_char_t *)"tag:yaml.org,2002:null", (yaml_char_t *)"null", 4, 1, 1, YAML_PLAIN_SCALAR_STYLE);
      yaml_emitter_emit(emitter, &event);
   }
}

// Write handler for emitter
static int yaml_write_handler(void *data, unsigned char *buffer, size_t size)
{
   char *output = (char *)data;
   size_t current_len = strlen(output);
   if (current_len + size < 4096)
   {
      memcpy(output + current_len, buffer, size);
      output[current_len + size] = '\0';
      return 1;
   }
   return 0;
}

// yaml.load(str)
static duk_ret_t duk_yaml_load(duk_context *ctx)
{
   if (!duk_is_string(ctx, 0))
   {
      duk_error(ctx, DUK_ERR_TYPE_ERROR, "input must be a string");
   }
   const char *input = duk_get_string(ctx, 0);

   yaml_parser_t parser;
   yaml_document_t doc;
   if (!yaml_parser_initialize(&parser))
   {
      duk_error(ctx, DUK_ERR_ERROR, "yaml_parser_initialize failed");
   }

   yaml_parser_set_input_string(&parser, (const unsigned char *)input, strlen(input));
   if (!yaml_parser_load(&parser, &doc))
   {
      yaml_parser_delete(&parser);
      duk_error(ctx, DUK_ERR_ERROR, "YAML parsing failed: %s", parser.error == YAML_MEMORY_ERROR ? "memory error" : parser.problem);
   }

   yaml_node_t *root = yaml_document_get_root_node(&doc);
   yaml_document_to_js(ctx, &doc, root);
   yaml_document_delete(&doc);
   yaml_parser_delete(&parser);
   return 1;
}

// yaml.dump(obj)
static duk_ret_t duk_yaml_dump(duk_context *ctx)
{
   yaml_emitter_t emitter;
   yaml_event_t event;
   if (!yaml_emitter_initialize(&emitter))
   {
      duk_error(ctx, DUK_ERR_ERROR, "yaml_emitter_initialize failed");
   }

   // Buffer to collect output
   char output[4096] = {0};
   yaml_emitter_set_output(&emitter, (yaml_write_handler_t *)yaml_write_handler, output);

   yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
   yaml_emitter_emit(&emitter, &event);
   yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
   yaml_emitter_emit(&emitter, &event);

   js_to_yaml_emitter(ctx, 0, &emitter);

   yaml_document_end_event_initialize(&event, 1);
   yaml_emitter_emit(&emitter, &event);
   yaml_stream_end_event_initialize(&event);
   yaml_emitter_emit(&emitter, &event);

   duk_push_string(ctx, output);
   yaml_emitter_delete(&emitter);
   return 1;
}

// YAML module functions
static const duk_function_list_entry yaml_functions[] = {
    {"load", duk_yaml_load, 1},
    {"dump", duk_yaml_dump, 1},
    {NULL, NULL, 0}};

// Module registration
duk_idx_t register_function(duk_context *ctx)
{
   duk_push_string(ctx, "yaml");
   duk_push_object(ctx);
   duk_put_function_list(ctx, -1, yaml_functions);
   duk_put_global_string(ctx, "yaml");

   return 1;
}
