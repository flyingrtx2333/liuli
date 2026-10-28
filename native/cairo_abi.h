#pragma once
// Small public Cairo ABI subset for this Linux prototype, not a Cairo replacement.
#if __has_include(<cairo/cairo.h>)
#include <cairo/cairo.h>
#elif __has_include(<cairo.h>)
#include <cairo.h>
#else
extern "C" {
struct _cairo; struct _cairo_surface;
using cairo_t = _cairo;
using cairo_surface_t = _cairo_surface;
enum cairo_format_t { CAIRO_FORMAT_ARGB32 = 0 };
enum cairo_status_t { CAIRO_STATUS_SUCCESS = 0 };
enum cairo_font_slant_t { CAIRO_FONT_SLANT_NORMAL = 0 };
enum cairo_font_weight_t { CAIRO_FONT_WEIGHT_NORMAL = 0, CAIRO_FONT_WEIGHT_BOLD = 1 };
cairo_surface_t* cairo_image_surface_create(cairo_format_t, int, int);
cairo_t* cairo_create(cairo_surface_t*);
cairo_status_t cairo_status(cairo_t*);
cairo_status_t cairo_surface_status(cairo_surface_t*);
const char* cairo_status_to_string(cairo_status_t);
cairo_status_t cairo_surface_write_to_png(cairo_surface_t*, const char*);
void cairo_destroy(cairo_t*);
void cairo_surface_destroy(cairo_surface_t*);
unsigned char* cairo_image_surface_get_data(cairo_surface_t*);
int cairo_image_surface_get_stride(cairo_surface_t*);
void cairo_surface_flush(cairo_surface_t*);
void cairo_surface_mark_dirty(cairo_surface_t*);
void cairo_scale(cairo_t*, double, double);
void cairo_set_source_surface(cairo_t*, cairo_surface_t*, double, double);
void cairo_set_source_rgba(cairo_t*, double, double, double, double);
void cairo_set_line_width(cairo_t*, double);
void cairo_stroke(cairo_t*);
void cairo_set_source_rgb(cairo_t*, double, double, double);
void cairo_paint(cairo_t*);
void cairo_rectangle(cairo_t*, double, double, double, double);
void cairo_fill(cairo_t*);
void cairo_move_to(cairo_t*, double, double);
void cairo_line_to(cairo_t*, double, double);
void cairo_arc(cairo_t*, double, double, double, double, double);
void cairo_close_path(cairo_t*);
void cairo_select_font_face(cairo_t*, const char*, cairo_font_slant_t, cairo_font_weight_t);
void cairo_set_font_size(cairo_t*, double);
struct cairo_text_extents_t {double x_bearing,y_bearing,width,height,x_advance,y_advance;};
void cairo_text_extents(cairo_t*,const char*,cairo_text_extents_t*);
void cairo_show_text(cairo_t*, const char*);
void cairo_save(cairo_t*);
void cairo_restore(cairo_t*);
void cairo_clip(cairo_t*);
}
#endif
