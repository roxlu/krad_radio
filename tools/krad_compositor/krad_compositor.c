#include "krad_compositor.h"

static void krad_compositor_close_display (krad_compositor_t *krad_compositor);
static void krad_compositor_open_display (krad_compositor_t *krad_compositor);
static void *krad_compositor_display_thread (void *arg);


static void *krad_compositor_display_thread (void *arg) {

	krad_compositor_t *krad_compositor = (krad_compositor_t *)arg;

	prctl (PR_SET_NAME, (unsigned long) "krad_display", 0, 0, 0);

	krad_x11_t *krad_x11;
	krad_frame_t *krad_frame;
	krad_compositor_port_t *krad_compositor_port;

	int w;
	int h;
	
	krad_compositor_get_resolution (krad_compositor,
							  &w,
							  &h);
	
	krad_compositor_port = krad_compositor_port_create (krad_compositor, "X11Out", OUTPUT,
														w, h);
	
	krad_x11 = krad_x11_create ();
	
	krad_x11_create_glx_window (krad_x11, "Krad Radio", w, h, NULL);
	
	while ((krad_x11->close_window == 0) && (krad_compositor->display_open == 1)) {
	
		krad_frame = krad_compositor_port_pull_frame (krad_compositor_port);

		if (krad_frame != NULL) {
			memcpy (krad_x11->pixels, krad_frame->pixels, w * h * 4);
			krad_framepool_unref_frame (krad_frame);
			krad_x11_glx_render (krad_x11);
		} else {
			krad_x11_glx_sync (krad_x11);
			krad_x11_glx_check_for_input (krad_x11);
		}
	}
	
	krad_compositor_port_destroy (krad_compositor, krad_compositor_port);
	
	krad_x11_destroy_glx_window (krad_x11);

	krad_x11_destroy (krad_x11);

	krad_compositor->display_open = 0;

	return NULL;

}


static void krad_compositor_open_display (krad_compositor_t *krad_compositor) {

	if (krad_compositor->display_open == 1) {
		krad_compositor_close_display (krad_compositor);
	}

	krad_compositor->display_open = 1;
	pthread_create (&krad_compositor->display_thread, NULL, krad_compositor_display_thread, (void *)krad_compositor);

}


static void krad_compositor_close_display (krad_compositor_t *krad_compositor) {

	if (krad_compositor->display_open == 1) {
		krad_compositor->display_open = 2;
		pthread_join (krad_compositor->display_thread, NULL);
		krad_compositor->display_open = 0;
	}

}

void krad_compositor_add_text (krad_compositor_t *krad_compositor, char *text, int x, int y, int tickrate, 
								 float scale, float opacity, float rotation, int red, int green, int blue, char *font) {

	int s;
	krad_text_t *krad_text;
	
	krad_text = NULL;

	for (s = 0; s < KRAD_COMPOSITOR_MAX_TEXTS; s++) {
		if (krad_compositor->krad_text[s].active == 0) {
			krad_text = &krad_compositor->krad_text[s];
			krad_text->active = 2;
			break;
		}
	}
	
	krad_text_set_xy (krad_text, x, y);
	krad_text_set_scale (krad_text, scale);
	krad_text_set_opacity (krad_text, opacity);
	krad_text_set_text (krad_text, text);	
	krad_text_set_rotation (krad_text, rotation);
	krad_text_set_tickrate (krad_text, tickrate);
	
	krad_text_set_font (krad_text, font);	

	krad_text_set_rgb (krad_text, red, green, blue);

	krad_text->active = 1;
	krad_compositor->active_texts++;

}

void krad_compositor_set_text (krad_compositor_t *krad_compositor, int num, int x, int y, int tickrate, 
								 float scale, float opacity, float rotation, int red, int green, int blue) {

	krad_text_t *krad_text;
	
	krad_text = NULL;

	krad_text = &krad_compositor->krad_text[num];

	krad_text_set_new_xy (krad_text, x, y);
	krad_text_set_new_scale (krad_text, scale);
	krad_text_set_new_opacity (krad_text, opacity);
	krad_text_set_new_rotation (krad_text, rotation);
	krad_text_set_tickrate (krad_text, tickrate);
	krad_text_set_new_rgb (krad_text, red, green, blue);

}

void krad_compositor_remove_text (krad_compositor_t *krad_compositor, int num) {

	krad_text_t *krad_text;
	
	krad_text = NULL;

	krad_text = &krad_compositor->krad_text[num];

	if (krad_text->active != 1) {
		return;
	}

	krad_text->active = 3;

	usleep (100000);
	krad_text_reset (krad_text);
	
	krad_text->active = 0;
	krad_compositor->active_texts--;

}


void krad_compositor_list_texts (krad_compositor_t *krad_compositor) {

}

void krad_compositor_add_sprite (krad_compositor_t *krad_compositor, char *filename, int x, int y, int tickrate, 
								 float scale, float opacity, float rotation) {

	int s;
	krad_sprite_t *krad_sprite;
	
	krad_sprite = NULL;

	for (s = 0; s < KRAD_COMPOSITOR_MAX_SPRITES; s++) {
		if (krad_compositor->krad_sprite[s].active == 0) {
			krad_sprite = &krad_compositor->krad_sprite[s];
			krad_sprite->active = 2;
			break;
		}
	}

	//krad_sprite = krad_sprite_create_from_file (filename);
	
	krad_sprite_open_file (krad_sprite, filename);
	
	krad_sprite_set_xy (krad_sprite, x, y);
	krad_sprite_set_scale (krad_sprite, scale);
	krad_sprite_set_new_opacity (krad_sprite, opacity);
	krad_sprite_set_rotation (krad_sprite, rotation);
	krad_sprite_set_tickrate (krad_sprite, tickrate);

	krad_sprite->active = 1;
	krad_compositor->active_sprites++;

}

void krad_compositor_set_sprite (krad_compositor_t *krad_compositor, int num, int x, int y, int tickrate, 
								 float scale, float opacity, float rotation) {

	krad_sprite_t *krad_sprite;
	
	krad_sprite = NULL;

	krad_sprite = &krad_compositor->krad_sprite[num];

	krad_sprite_set_new_xy (krad_sprite, x, y);
	krad_sprite_set_new_scale (krad_sprite, scale);
	krad_sprite_set_new_opacity (krad_sprite, opacity);
	krad_sprite_set_new_rotation (krad_sprite, rotation);
	krad_sprite_set_tickrate (krad_sprite, tickrate);

}

void krad_compositor_remove_sprite (krad_compositor_t *krad_compositor, int num) {

	krad_sprite_t *krad_sprite;
	
	krad_sprite = NULL;

	krad_sprite = &krad_compositor->krad_sprite[num];

	if (krad_sprite->active != 1) {
		return;
	}

	krad_sprite->active = 3;

	usleep (100000);
	krad_sprite_reset (krad_sprite);
	
	krad_sprite->active = 0;
	krad_compositor->active_sprites--;

}


void krad_compositor_list_sprites (krad_compositor_t *krad_compositor) {

}


void krad_compositor_unset_background (krad_compositor_t *krad_compositor) {

	if (krad_compositor->background != NULL) {
		if (krad_compositor->background_pattern != NULL) {
			cairo_pattern_destroy (krad_compositor->background_pattern);
			krad_compositor->background_pattern = NULL;
		}
		cairo_surface_destroy ( krad_compositor->background );
		krad_compositor->background = NULL;
		krad_compositor->background_width = 0;
		krad_compositor->background_height = 0;
	}
}


void krad_compositor_set_background (krad_compositor_t *krad_compositor, char *filename) {

	if (krad_compositor->background != NULL) {
		krad_compositor_unset_background (krad_compositor);
	}
	
	if ( filename != NULL ) {
		krad_compositor->background = cairo_image_surface_create_from_png ( filename );
		
		if (cairo_surface_status (krad_compositor->background) != CAIRO_STATUS_SUCCESS) {
			krad_compositor->background = NULL;
			return;
		}
		krad_compositor->background_width = cairo_image_surface_get_width ( krad_compositor->background );
		krad_compositor->background_height = cairo_image_surface_get_height ( krad_compositor->background );
		if ((krad_compositor->background_width != krad_compositor->width) ||
		    (krad_compositor->background_height != krad_compositor->height)) {

			krad_compositor->background_pattern = cairo_pattern_create_for_surface (krad_compositor->background);
			cairo_pattern_set_extend (krad_compositor->background_pattern, CAIRO_EXTEND_REPEAT);
		}
	}
}


void krad_compositor_render_background (krad_compositor_t *krad_compositor, krad_frame_t *frame) {

	if (krad_compositor->background_pattern != NULL) {
		cairo_set_source (krad_compositor->krad_gui->cr, krad_compositor->background_pattern);
	} else {
		cairo_set_source_surface ( krad_compositor->krad_gui->cr, krad_compositor->background, 0, 0 );
	}
	cairo_paint ( krad_compositor->krad_gui->cr );

}


void krad_compositor_process (krad_compositor_t *krad_compositor) {

	int p;
	//int need_clear_or_background;
	krad_frame_t *composite_frame;
	krad_frame_t *frame;	
	
	frame = NULL;	
	composite_frame = NULL;

	krad_compositor->timecode = round (1000000000 * krad_compositor->frame_num / krad_compositor->frame_rate_numerator * krad_compositor->frame_rate_denominator / 1000000);
	krad_compositor->frame_num++;
	
	if (krad_compositor->active_ports < 1) {
		return;
	}

	//printk ("timecode is %llu", krad_compositor->timecode);
	
	if (krad_compositor->bug_filename != NULL) {
		if (strlen(krad_compositor->bug_filename)) {
			printk ("setting bug to %s %d %d", krad_compositor->bug_filename, 
			        krad_compositor->bug_x, krad_compositor->bug_y);
			krad_gui_set_bug (krad_compositor->krad_gui, krad_compositor->bug_filename, 
							 krad_compositor->bug_x, krad_compositor->bug_y);
		} else {
			printk ("removing bug");
			krad_gui_remove_bug (krad_compositor->krad_gui);
		}
		free (krad_compositor->bug_filename);
		krad_compositor->bug_filename = NULL;
	}
	
	/* Get a frame */
	
	do {
		composite_frame = krad_framepool_getframe (krad_compositor->krad_framepool);
		if (composite_frame == NULL) {
			printke ("This is very bad! Compositor wanted a frame but could not get one right away!");
			usleep (5000);
		}
	} while (composite_frame == NULL);	
	
	krad_gui_set_surface (krad_compositor->krad_gui, composite_frame->cst);
	
	krad_gui_clear (krad_compositor->krad_gui);
	
	if (krad_compositor->active_input_ports == 0) {
	
		if (krad_compositor->background != NULL) {
			cairo_save (krad_compositor->krad_gui->cr);
			krad_compositor_render_background (krad_compositor, composite_frame);
			cairo_restore (krad_compositor->krad_gui->cr);
		} else {

			//krad_gui_clear (krad_compositor->krad_gui);	
		
			if ((krad_compositor->active_sprites == 0) && (krad_compositor->active_texts == 0)) {
			
				krad_compositor->no_input++;
			
				if ((krad_compositor->no_input > 140) && ((krad_compositor->frame_num % 30) > 15)) {
					cairo_save (krad_compositor->krad_gui->cr);
					cairo_set_source_rgb (krad_compositor->krad_gui->cr, GREY);
					cairo_select_font_face (krad_compositor->krad_gui->cr, "monospace",
											CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
					cairo_set_font_size (krad_compositor->krad_gui->cr, 96.0);
					cairo_move_to (krad_compositor->krad_gui->cr, krad_compositor->krad_gui->width/3.0,
								   krad_compositor->krad_gui->height/2);
					cairo_show_text (krad_compositor->krad_gui->cr, "NO INPUT");
					cairo_restore (krad_compositor->krad_gui->cr);
				}
			} else {
				krad_compositor->no_input = 0;
			}
		}
		
	} else {
	
		krad_compositor->no_input = 0;
		/*
		need_clear_or_background = 1;

		for (p = 0; p < KRAD_COMPOSITOR_MAX_PORTS; p++) {
			if ((krad_compositor->port[p].active == 1) && (krad_compositor->port[p].direction == INPUT)) {
				if ((krad_compositor->port[p].x == 0) && (krad_compositor->port[p].y == 0) &&
					(krad_compositor->port[p].crop_width == krad_compositor->width) &&
					(krad_compositor->port[p].crop_height == krad_compositor->height)) {
					
					need_clear_or_background = 0;
					break;
				}
			}
		}

		if (need_clear_or_background == 1) {
		*/
			if (krad_compositor->background != NULL) {
				cairo_save (krad_compositor->krad_gui->cr);
				krad_compositor_render_background (krad_compositor, composite_frame);
				cairo_restore (krad_compositor->krad_gui->cr);
			} else {		
				//krad_gui_clear (krad_compositor->krad_gui);
			}
		//}

		/* Composite Input Ports */

		for (p = 0; p < KRAD_COMPOSITOR_MAX_PORTS; p++) {
			if ((krad_compositor->port[p].active == 1) && (krad_compositor->port[p].direction == INPUT)) {

				frame = krad_compositor_port_pull_frame (&krad_compositor->port[p]);		

				if (frame != NULL) {
				
					if (krad_compositor->port[p].opacity == 0.0f) {
						krad_framepool_unref_frame (frame);
						continue;
					}
			
					cairo_save (krad_compositor->krad_gui->cr);
					if (krad_compositor->port[p].rotation != 0.0f) {
						cairo_translate (krad_compositor->krad_gui->cr,
										 krad_compositor->port[p].crop_width / 2,
										 krad_compositor->port[p].crop_height / 2);
										 
						cairo_rotate (krad_compositor->krad_gui->cr,
									  krad_compositor->port[p].rotation * (M_PI/180.0));

						cairo_translate (krad_compositor->krad_gui->cr,
										 krad_compositor->port[p].crop_width / -2,
										 krad_compositor->port[p].crop_height / -2);
					}
					cairo_set_source_surface (krad_compositor->krad_gui->cr,
											  frame->cst,
											  krad_compositor->port[p].x - krad_compositor->port[p].crop_x,
											  krad_compositor->port[p].y - krad_compositor->port[p].crop_y);

					cairo_rectangle (krad_compositor->krad_gui->cr,
									 krad_compositor->port[p].x,
									 krad_compositor->port[p].y,
									 krad_compositor->port[p].crop_width,
									 krad_compositor->port[p].crop_height);
					cairo_clip (krad_compositor->krad_gui->cr);
					
					if (krad_compositor->krad_text[1].active == 1) {
					
						cairo_mask_surface (krad_compositor->krad_gui->cr, krad_compositor->mask_cst, 0, 0);
					
					} else {		 
						if (krad_compositor->port[p].opacity == 1.0f) {
							cairo_paint (krad_compositor->krad_gui->cr);
						} else {
							cairo_paint_with_alpha (krad_compositor->krad_gui->cr, krad_compositor->port[p].opacity);
						}
					}
					cairo_restore (krad_compositor->krad_gui->cr);
					krad_framepool_unref_frame (frame);
				}
			}
		}
	}

	/* Render Overlayed Items */
	
	if (krad_compositor->hex_size > 0) {
		krad_gui_render_hex (krad_compositor->krad_gui, krad_compositor->hex_x, krad_compositor->hex_y, 
		                    krad_compositor->hex_size);	
	} else {
	//	krad_gui_render_selector (krad_compositor->krad_gui, krad_compositor->hex_x, krad_compositor->hex_y, 
	//	                    64);
	
	//	krad_gui_render_meter (krad_compositor->krad_gui, krad_compositor->krad_gui->width / 2,
	//	                      krad_compositor->krad_gui->height/3, 264, krad_system_get_cpu_usage ());
	//
	}
	
	if (krad_compositor->render_vu_meters > 0) {
		krad_gui_render_meter (krad_compositor->krad_gui, 110, krad_compositor->krad_gui->height - 30, 64,
		                      krad_compositor->krad_gui->output_current[0]);
		krad_gui_render_meter (krad_compositor->krad_gui, krad_compositor->krad_gui->width - 110,
		                      krad_compositor->krad_gui->height - 30, 64, krad_compositor->krad_gui->output_current[1]);
	}
	

	for (p = 0; p < KRAD_COMPOSITOR_MAX_SPRITES; p++) {
		if (krad_compositor->krad_sprite[p].active == 1) {
			krad_sprite_render (&krad_compositor->krad_sprite[p], krad_compositor->krad_gui->cr);
		}
	}
	
	for (p = 0; p < KRAD_COMPOSITOR_MAX_TEXTS; p++) {
		if (krad_compositor->krad_text[p].active == 1) {
			//if (p == 1) {
			//	cairo_save (krad_compositor->mask_cr);
			//	cairo_set_operator (krad_compositor->mask_cr, CAIRO_OPERATOR_CLEAR);	
			//	cairo_paint (krad_compositor->mask_cr);
			//	cairo_restore (krad_compositor->mask_cr);
			//	krad_text_expand (&krad_compositor->krad_text[p], krad_compositor->krad_gui->cr, krad_compositor->width);
			//	krad_text_render (&krad_compositor->krad_text[p], krad_compositor->mask_cr);
			//} else {
				krad_text_render (&krad_compositor->krad_text[p], krad_compositor->krad_gui->cr);
			//}
		}
	}	

	krad_gui_render (krad_compositor->krad_gui);

	/* Push out the composited frame */
	
	for (p = 0; p < KRAD_COMPOSITOR_MAX_PORTS; p++) {
		if ((krad_compositor->port[p].active == 1) && (krad_compositor->port[p].direction == OUTPUT)) {
			krad_compositor_port_push_frame (&krad_compositor->port[p], composite_frame);
		}
	}
	
	if (krad_compositor->snapshot > 0) {
		krad_compositor_take_snapshot (krad_compositor, composite_frame);
	}
	
	krad_framepool_unref_frame (composite_frame);
	
}

void *krad_compositor_snapshot_thread (void *arg) {

	krad_compositor_snapshot_t *krad_compositor_snapshot = (krad_compositor_snapshot_t *)arg;
	
	//printk ("SNAPPY! %s", krad_compositor_snapshot->filename);
	
	cairo_surface_write_to_png (krad_compositor_snapshot->krad_frame->cst, krad_compositor_snapshot->filename);
	
	krad_framepool_unref_frame (krad_compositor_snapshot->krad_frame);
	free (krad_compositor_snapshot);

	return NULL;

}

void krad_compositor_take_snapshot (krad_compositor_t *krad_compositor, krad_frame_t *krad_frame) {
		
	krad_compositor_snapshot_t *krad_compositor_snapshot;

	krad_compositor->snapshot--;

	if (krad_compositor->dir == NULL) {	
		return;
	}

	krad_framepool_ref_frame (krad_frame);
		
	krad_compositor_snapshot = calloc (1, sizeof (krad_compositor_snapshot_t));
	
	krad_compositor_snapshot->krad_frame = krad_frame;
	
	sprintf (krad_compositor_snapshot->filename,
			 "%s/snapshot_%zu_%"PRIu64".png",
			 krad_compositor->dir,
			 time (NULL),
			 krad_compositor->frame_num);
	
	pthread_create (&krad_compositor->snapshot_thread,
					NULL,
					krad_compositor_snapshot_thread,
				    (void *)krad_compositor_snapshot);

	pthread_detach (krad_compositor->snapshot_thread);

}


void krad_compositor_mjpeg_process (krad_compositor_t *krad_compositor) {

	int p;
	
	krad_frame_t *krad_frame;
	
	krad_frame = NULL;

	for (p = 0; p < KRAD_COMPOSITOR_MAX_PORTS; p++) {
		if ((krad_compositor->port[p].active == 1) && (krad_compositor->port[p].direction == INPUT) &&
			(krad_compositor->port[p].mjpeg == 1)) {
				krad_frame = krad_compositor_port_pull_frame (&krad_compositor->port[p]);
			break;
		}
	}
	
	if (krad_frame == NULL) {
		return;
	}

	for (p = 0; p < KRAD_COMPOSITOR_MAX_PORTS; p++) {
		if ((krad_compositor->port[p].active == 1) && (krad_compositor->port[p].direction == OUTPUT) &&
			(krad_compositor->port[p].mjpeg == 1)) {

			krad_compositor_port_push_frame (&krad_compositor->port[p], krad_frame);
			break;
		}
	}
	
	krad_framepool_unref_frame (krad_frame);

}

void krad_compositor_port_set_comp_params (krad_compositor_port_t *krad_compositor_port,
										   int width, int height, int x, int y, 
										   int crop_width, int crop_height,
										   int crop_x, int crop_y, float opacity, float rotation) {
										   

	krad_compositor_port->width = width;
	krad_compositor_port->height = height;

	krad_compositor_port->crop_width = crop_width;
	krad_compositor_port->crop_height = crop_height;
	
	krad_compositor_port->crop_x = crop_x;
	krad_compositor_port->crop_y = crop_y;	

	krad_compositor_port->x = x;
	krad_compositor_port->y = y;

	krad_compositor_port->opacity = opacity;
	krad_compositor_port->rotation = rotation;

	krad_compositor_port->comp_params_updated = 1;
										   
}

void krad_compositor_port_set_io_params (krad_compositor_port_t *krad_compositor_port,
										 int width, int height) {
										 
	krad_compositor_port->source_width = width;
	krad_compositor_port->source_height = height;


	krad_compositor_aspect_scale (krad_compositor_port->source_width, krad_compositor_port->source_height,
								  krad_compositor_port->krad_compositor->width, krad_compositor_port->krad_compositor->height,
								  &krad_compositor_port->width, &krad_compositor_port->height);

	krad_compositor_port->crop_width = krad_compositor_port->width;
	krad_compositor_port->crop_height = krad_compositor_port->height;

	if (krad_compositor_port->width < krad_compositor_port->krad_compositor->width) {
		krad_compositor_port->x = (krad_compositor_port->krad_compositor->width - krad_compositor_port->width) / 2;
	}
	
	if (krad_compositor_port->height < krad_compositor_port->krad_compositor->height) {
		krad_compositor_port->y = (krad_compositor_port->krad_compositor->height - krad_compositor_port->height) / 2;
	}	

	krad_compositor_port->io_params_updated = 1;
										   
}										   

void krad_compositor_port_push_yuv_frame (krad_compositor_port_t *krad_compositor_port, krad_frame_t *krad_frame) {

	int rgb_stride_arr[3] = {4*krad_compositor_port->krad_compositor->width, 0, 0};
	unsigned char *dst[4];
	
	if ((krad_compositor_port->io_params_updated) || (krad_compositor_port->comp_params_updated)) {
		if (krad_compositor_port->sws_converter != NULL) {
			sws_freeContext ( krad_compositor_port->sws_converter );
			krad_compositor_port->sws_converter = NULL;
		}
		krad_compositor_port->io_params_updated = 0;
		krad_compositor_port->comp_params_updated = 0;
	}
	
	if (krad_compositor_port->sws_converter == NULL) {

		krad_compositor_port->sws_converter =
			sws_getContext ( krad_compositor_port->source_width,
							 krad_compositor_port->source_height,
							 krad_frame->format,
							 krad_compositor_port->width,
							 krad_compositor_port->height,
							 PIX_FMT_RGB32, 
							 SWS_BICUBIC,
							 NULL, NULL, NULL);
							 
		printk ("set scaling to w %d h %d sw %d sh %d",
				krad_compositor_port->width,
				krad_compositor_port->height,
				krad_compositor_port->source_width,
				krad_compositor_port->source_height);							 
							 

	}									 

	dst[0] = (unsigned char *)krad_frame->pixels;

	sws_scale (krad_compositor_port->sws_converter, (const uint8_t * const*)krad_frame->yuv_pixels,
			   krad_frame->yuv_strides, 0, krad_compositor_port->source_height, dst, rgb_stride_arr);

	krad_compositor_port_push_frame (krad_compositor_port, krad_frame);

}

krad_frame_t *krad_compositor_port_pull_yuv_frame (krad_compositor_port_t *krad_compositor_port,
												   uint8_t *yuv_pixels[4], int yuv_strides[4]) {

	krad_frame_t *krad_frame;	
	
	if (krad_ringbuffer_read_space (krad_compositor_port->frame_ring) >= sizeof(krad_frame_t *)) {
		krad_ringbuffer_read (krad_compositor_port->frame_ring, (char *)&krad_frame, sizeof(krad_frame_t *));
		
		
		
		
	int rgb_stride_arr[3] = {4*krad_compositor_port->krad_compositor->width, 0, 0};
	unsigned char *src[4];
	
	if (krad_compositor_port->io_params_updated) {
		if (krad_compositor_port->sws_converter != NULL) {
			sws_freeContext ( krad_compositor_port->sws_converter );
			krad_compositor_port->sws_converter = NULL;
		}
		krad_compositor_port->io_params_updated = 0;
	}
	
	if (krad_compositor_port->sws_converter == NULL) {

		krad_compositor_port->sws_converter =
			sws_getContext ( krad_compositor_port->krad_compositor->width,
							 krad_compositor_port->krad_compositor->height,
							 PIX_FMT_RGB32,
							 krad_compositor_port->width,
							 krad_compositor_port->height,
							 PIX_FMT_YUV420P, 
							 SWS_BICUBIC,
							 NULL, NULL, NULL);

	}									 

	src[0] = (unsigned char *)krad_frame->pixels;

	sws_scale (krad_compositor_port->sws_converter, (const uint8_t * const*)src,
			   rgb_stride_arr, 0, krad_compositor_port->krad_compositor->height, yuv_pixels, yuv_strides);
		
		return krad_frame;
	}

	return NULL;

}

void krad_compositor_port_push_rgba_frame (krad_compositor_port_t *krad_compositor_port, krad_frame_t *krad_frame) {


	int output_rgb_stride_arr[4] = {4*krad_compositor_port->krad_compositor->width, 0, 0, 0};
	int input_rgb_stride_arr[4] = {4*krad_compositor_port->source_width, 0, 0, 0};	
	unsigned char *dst[4];
	unsigned char *src[4];	
	krad_frame_t *scaled_frame;	
	
	krad_frame->format = PIX_FMT_RGB32;
	
	if ((krad_compositor_port->source_width != krad_compositor_port->width) ||
		(krad_compositor_port->source_height != krad_compositor_port->height)) {
	
		if ((krad_compositor_port->io_params_updated) || (krad_compositor_port->comp_params_updated)) {
			if (krad_compositor_port->sws_converter != NULL) {
				sws_freeContext ( krad_compositor_port->sws_converter );
				krad_compositor_port->sws_converter = NULL;
			}
			krad_compositor_port->io_params_updated = 0;
			krad_compositor_port->comp_params_updated = 0;
		}
	
		if (krad_compositor_port->sws_converter == NULL) {

			krad_compositor_port->sws_converter =
				sws_getContext ( krad_compositor_port->source_width,
								 krad_compositor_port->source_height,
								 krad_frame->format,
								 krad_compositor_port->width,
								 krad_compositor_port->height,
								 PIX_FMT_RGB32, 
								 SWS_BICUBIC,
								 NULL, NULL, NULL);
				
			if (krad_compositor_port->sws_converter == NULL) {
				failfast ("Krad Compositor: fraked");
			}
								 
			printk ("set scaling to w %d h %d sw %d sh %d",
					krad_compositor_port->width,
					krad_compositor_port->height,
					krad_compositor_port->source_width,
					krad_compositor_port->source_height);							 
								 

		}
		
		scaled_frame = krad_framepool_getframe (krad_compositor_port->krad_compositor->krad_framepool);					 

		if (scaled_frame == NULL) {
			failfast ("Krad Compositor: out of frames");
		}

		src[0] = (unsigned char *)krad_frame->pixels;
		dst[0] = (unsigned char *)scaled_frame->pixels;

		sws_scale (krad_compositor_port->sws_converter, (const uint8_t * const*)src,
				   input_rgb_stride_arr, 0, krad_compositor_port->source_height, dst, output_rgb_stride_arr);
				   

		krad_compositor_port_push_frame (krad_compositor_port, scaled_frame);
		
		krad_framepool_unref_frame (scaled_frame);
	
	} else {

		krad_compositor_port_push_frame (krad_compositor_port, krad_frame);

	}

}

void krad_compositor_port_push_frame (krad_compositor_port_t *krad_compositor_port, krad_frame_t *krad_frame) {

	krad_framepool_ref_frame (krad_frame);
	
	krad_ringbuffer_write (krad_compositor_port->frame_ring, (char *)&krad_frame, sizeof(krad_frame_t *));

}


krad_frame_t *krad_compositor_port_pull_frame (krad_compositor_port_t *krad_compositor_port) {

	krad_frame_t *krad_frame;

	if (krad_compositor_port->direction == INPUT) {


		if (krad_compositor_port->last_frame != NULL) {

			//printk ("%llu -- %llu", krad_compositor_port->start_timecode + krad_compositor_port->last_frame->timecode,
			//krad_compositor_port->krad_compositor->timecode);

			if ((krad_compositor_port->start_timecode + krad_compositor_port->last_frame->timecode) < 
				krad_compositor_port->krad_compositor->timecode) {

				if (krad_ringbuffer_read_space (krad_compositor_port->frame_ring) >= sizeof(krad_frame_t *)) {
					krad_ringbuffer_read (krad_compositor_port->frame_ring, (char *)&krad_frame, sizeof(krad_frame_t *));

					krad_framepool_unref_frame (krad_compositor_port->last_frame);	
					krad_framepool_ref_frame (krad_frame);
					krad_compositor_port->last_frame = krad_frame;
		
					return krad_frame;
				} else {
					krad_framepool_ref_frame (krad_compositor_port->last_frame);
					return krad_compositor_port->last_frame;
				}

			} else {
				krad_framepool_ref_frame (krad_compositor_port->last_frame);
				return krad_compositor_port->last_frame;
			}
		} else {
		
			if (krad_ringbuffer_read_space (krad_compositor_port->frame_ring) >= sizeof(krad_frame_t *)) {
				krad_ringbuffer_read (krad_compositor_port->frame_ring, (char *)&krad_frame, sizeof(krad_frame_t *));
	
				krad_compositor_port->start_timecode = krad_compositor_port->krad_compositor->timecode;
	
				krad_framepool_ref_frame (krad_frame);
				krad_compositor_port->last_frame = krad_frame;
	
				return krad_frame;
			}
		
		
		}
		
	} else {
		
		if (krad_ringbuffer_read_space (krad_compositor_port->frame_ring) >= sizeof(krad_frame_t *)) {
			krad_ringbuffer_read (krad_compositor_port->frame_ring, (char *)&krad_frame, sizeof(krad_frame_t *));			
			return krad_frame;
		}
	}

	return NULL;

}

int krad_compositor_port_frames_avail (krad_compositor_port_t *krad_compositor_port) {
	
	int frames;
	
	frames = krad_ringbuffer_read_space (krad_compositor_port->frame_ring);
	
	frames = frames / sizeof (krad_frame_t *);

	return frames;
}

void krad_compositor_relloc_resources (krad_compositor_t *krad_compositor) {
	krad_compositor_free_resources (krad_compositor);
	krad_compositor_alloc_resources (krad_compositor);
}

void krad_compositor_free_resources (krad_compositor_t *krad_compositor) {

	if (krad_compositor->mask_cst != NULL) {
		cairo_surface_destroy (krad_compositor->mask_cst);
		krad_compositor->mask_cst = NULL;
		if (krad_compositor->mask_cr != NULL) {
			cairo_destroy (krad_compositor->mask_cr);
			krad_compositor->mask_cr = NULL;
		}
	}

	if (krad_compositor->krad_framepool != NULL) {
		krad_framepool_destroy ( krad_compositor->krad_framepool );
		krad_compositor->krad_framepool = NULL;
	}

	if (krad_compositor->krad_gui != NULL) {
		krad_gui_destroy (krad_compositor->krad_gui);
		krad_compositor->krad_gui = NULL;
	}
}


void krad_compositor_alloc_resources (krad_compositor_t *krad_compositor) {

	if (krad_compositor->mask_cst == NULL) {
		krad_compositor->mask_cst = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
																krad_compositor->width,
																krad_compositor->height);
		krad_compositor->mask_cr = cairo_create (krad_compositor->mask_cst);															
	}

	if (krad_compositor->krad_framepool == NULL) {
		krad_compositor->krad_framepool = 
			krad_framepool_create ( krad_compositor->width, krad_compositor->height, DEFAULT_COMPOSITOR_BUFFER_FRAMES);
	}
	
	if (krad_compositor->krad_gui == NULL) {
		krad_compositor->krad_gui = krad_gui_create (krad_compositor->width, krad_compositor->height);
		//krad_compositor->krad_gui = krad_gui_create_with_internal_surface (krad_compositor->width, krad_compositor->height);
		//krad_compositor->krad_gui = krad_gui_create_with_external_surface (krad_compositor->width,
		//																  krad_compositor->height,
		//																  NULL);
		krad_compositor->krad_gui->clear = 0;
	}
		
}

void krad_compositor_aspect_scale (int width, int height,
								   int avail_width, int avail_height,
								   int *new_width, int *new_heigth) {
	
	double scaleX, scaleY, scale;

	scaleX = (float)avail_width  / width;  
	scaleY = (float)avail_height / height;  
	scale = MIN ( scaleX, scaleY );
	
	*new_width = width * scale;
	*new_heigth = height * scale;
	
	printk ("Source: %d x %d Max: %d x %d Aspect Constrained: %d x %d",
			width, height,
			avail_width, avail_height,
			*new_width, *new_heigth);
	
}


krad_compositor_port_t *krad_compositor_port_create (krad_compositor_t *krad_compositor, char *sysname, int direction,
													 int width, int height) {

	krad_compositor_port_t *krad_compositor_port;

	int p;

	pthread_mutex_lock (&krad_compositor->settings_lock);

	for (p = 0; p < KRAD_COMPOSITOR_MAX_PORTS; p++) {
		if (krad_compositor->port[p].active == 0) {
			krad_compositor_port = &krad_compositor->port[p];
			krad_compositor_port->active = 2;
			break;
		}
	}
	
	if (krad_compositor_port == NULL) {
		return NULL;
	}
	
	krad_compositor_port->direction = direction;	

	if (krad_compositor_port->direction == INPUT) {
		krad_compositor_port->source_width = width;
		krad_compositor_port->source_height = height;
		
		krad_compositor_aspect_scale (krad_compositor_port->source_width, krad_compositor_port->source_height,
									  krad_compositor->width, krad_compositor->height,
									  &krad_compositor_port->width, &krad_compositor_port->height);
			
	} else {
		krad_compositor_port->source_width = krad_compositor->width;
		krad_compositor_port->source_height = krad_compositor->height;
		krad_compositor_port->width = width;
		krad_compositor_port->height = height;	
	}
	
	krad_compositor_port->x = 0;
	krad_compositor_port->y = 0;	
	krad_compositor_port->crop_x = 0;
	krad_compositor_port->crop_y = 0;
	krad_compositor_port->crop_width = krad_compositor->width;
	krad_compositor_port->crop_height = krad_compositor->height;
	
	krad_compositor_port->opacity = 1.0f;
	krad_compositor_port->rotation = 0.0f;
	
	krad_compositor_alloc_resources (krad_compositor);
	
	krad_compositor_port->krad_compositor = krad_compositor;
	
	strcpy (krad_compositor_port->sysname, sysname);	
	
	krad_compositor_port->start_timecode = 1;
	
	krad_compositor_port->frame_ring = 
		krad_ringbuffer_create ( DEFAULT_COMPOSITOR_BUFFER_FRAMES * sizeof(krad_frame_t *) );
	
	krad_compositor_port->active = 1;
	
	krad_compositor->active_ports++;
	
	if (krad_compositor_port->direction == INPUT) {
		krad_compositor->active_input_ports++;
	}
	if (krad_compositor_port->direction == OUTPUT) {
		krad_compositor->active_output_ports++;
	}
	pthread_mutex_unlock (&krad_compositor->settings_lock);		
	
	return krad_compositor_port;

}

krad_compositor_port_t *krad_compositor_mjpeg_port_create (krad_compositor_t *krad_compositor,
														   char *sysname, int direction) {

	krad_compositor_port_t *krad_compositor_port;
	
	krad_compositor_port = krad_compositor_port_create (krad_compositor, sysname, direction,
														1,1);

	krad_compositor_port->mjpeg = 1;		

	return krad_compositor_port;	

}


void krad_compositor_port_destroy (krad_compositor_t *krad_compositor, krad_compositor_port_t *krad_compositor_port) {

	pthread_mutex_lock (&krad_compositor->settings_lock);	
	krad_compositor_port->active = 3;

	if (krad_compositor_port->direction == INPUT) {
		krad_compositor->active_input_ports--;
	}
	if (krad_compositor_port->direction == OUTPUT) {
		krad_compositor->active_output_ports--;
	}

	krad_ringbuffer_free ( krad_compositor_port->frame_ring );
	krad_compositor_port->start_timecode = 0;
	krad_compositor_port->active = 0;

	if (krad_compositor_port->sws_converter != NULL) {
		sws_freeContext ( krad_compositor_port->sws_converter );
		krad_compositor_port->sws_converter = NULL;
	}

	if (krad_compositor_port->last_frame != NULL) {
		krad_framepool_unref_frame (krad_compositor_port->last_frame);
		krad_compositor_port->last_frame = NULL;
	}

	krad_compositor->active_ports--;
	pthread_mutex_unlock (&krad_compositor->settings_lock);	

}

void krad_compositor_destroy (krad_compositor_t *krad_compositor) {

	int p;

	for (p = 0; p < KRAD_COMPOSITOR_MAX_PORTS; p++) {
		if (krad_compositor->port[p].active == 1) {
			krad_compositor_port_destroy (krad_compositor, &krad_compositor->port[p]);
		}
	}
	
	krad_compositor_free_resources (krad_compositor);
	
	pthread_mutex_destroy (&krad_compositor->settings_lock);	

	free (krad_compositor->port);
	free (krad_compositor->krad_sprite);
	free (krad_compositor->krad_text);	
	free (krad_compositor);

}

void krad_compositor_get_resolution (krad_compositor_t *krad_compositor, int *width, int *height) {

	pthread_mutex_lock (&krad_compositor->settings_lock);

	*width = krad_compositor->width;
	*height = krad_compositor->height;

	pthread_mutex_unlock (&krad_compositor->settings_lock);
}

void krad_compositor_get_frame_rate (krad_compositor_t *krad_compositor,
									 int *frame_rate_numerator, int *frame_rate_denominator) {

	pthread_mutex_lock (&krad_compositor->settings_lock);
	
	*frame_rate_numerator = krad_compositor->frame_rate_numerator;
	*frame_rate_denominator = krad_compositor->frame_rate_denominator;

	pthread_mutex_unlock (&krad_compositor->settings_lock);

}

void krad_compositor_update_resolution (krad_compositor_t *krad_compositor, int width, int height) {

	pthread_mutex_lock (&krad_compositor->settings_lock);
	if ((krad_compositor->active_input_ports == 0) && (krad_compositor->active_output_ports == 0)) {
		krad_compositor_set_resolution (krad_compositor, width, height);
		krad_compositor_relloc_resources (krad_compositor);
	}
	pthread_mutex_unlock (&krad_compositor->settings_lock);	
	
}

void krad_compositor_set_resolution (krad_compositor_t *krad_compositor, int width, int height) {

	krad_compositor->width = width;
	krad_compositor->height = height;

	krad_compositor->frame_byte_size = krad_compositor->width * krad_compositor->height * 4;

}

krad_compositor_t *krad_compositor_create (int width, int height,
										   int frame_rate_numerator, int frame_rate_denominator) {

	int i;

	krad_compositor_t *krad_compositor = calloc(1, sizeof(krad_compositor_t));

	krad_compositor_set_resolution (krad_compositor, width, height);

	krad_compositor->port = calloc(KRAD_COMPOSITOR_MAX_PORTS, sizeof(krad_compositor_port_t));
	
	krad_compositor->krad_sprite = calloc(KRAD_COMPOSITOR_MAX_SPRITES, sizeof(krad_sprite_t));	

	krad_compositor->krad_text = calloc(KRAD_COMPOSITOR_MAX_TEXTS, sizeof(krad_text_t));
	
	
	for (i = 0; i < KRAD_COMPOSITOR_MAX_SPRITES; i++) {
		krad_sprite_reset (&krad_compositor->krad_sprite[i]);
	}
	
	for (i = 0; i < KRAD_COMPOSITOR_MAX_TEXTS; i++) {
		krad_text_reset (&krad_compositor->krad_text[i]);
	}
	
	pthread_mutex_init (&krad_compositor->settings_lock, NULL);
	
	krad_compositor_set_frame_rate (krad_compositor, frame_rate_numerator, frame_rate_denominator);
	
	krad_compositor->hex_x = 150;
	krad_compositor->hex_y = 100;
	krad_compositor->hex_size = 0;

	krad_compositor->bug_filename = NULL;	
	
	krad_compositor->render_vu_meters = 0;
	
	krad_compositor_alloc_resources (krad_compositor);
	
	//krad_compositor_start_ticker (krad_compositor);
		
	return krad_compositor;

}


void *krad_compositor_ticker_thread (void *arg) {

	krad_compositor_t *krad_compositor = (krad_compositor_t *)arg;

	prctl (PR_SET_NAME, (unsigned long) "krad_compositor", 0, 0, 0);

	krad_compositor->krad_ticker = krad_ticker_create (krad_compositor->frame_rate_numerator,
													   krad_compositor->frame_rate_denominator);
													   
	krad_ticker_start (krad_compositor->krad_ticker);

	while (krad_compositor->ticker_running == 1) {
	
		krad_compositor_process (krad_compositor);
	
		krad_ticker_wait (krad_compositor->krad_ticker);

	}

	krad_ticker_destroy (krad_compositor->krad_ticker);


	return NULL;

}


void krad_compositor_start_ticker (krad_compositor_t *krad_compositor) {

	if (krad_compositor->ticker_running == 1) {
		krad_compositor_stop_ticker (krad_compositor);
	}

	krad_compositor->ticker_running = 1;
	pthread_create (&krad_compositor->ticker_thread, NULL, krad_compositor_ticker_thread, (void *)krad_compositor);

}


void krad_compositor_stop_ticker (krad_compositor_t *krad_compositor) {

	if (krad_compositor->ticker_running == 1) {
		krad_compositor->ticker_running = 2;
		pthread_join (krad_compositor->ticker_thread, NULL);
		krad_compositor->ticker_running = 0;
	}

}

void krad_compositor_unset_pusher (krad_compositor_t *krad_compositor) {
	if (krad_compositor->ticker_running == 1) {
		krad_compositor_stop_ticker (krad_compositor);
	}
	krad_compositor_start_ticker (krad_compositor);
	krad_compositor->pusher = 0;
}

void krad_compositor_set_pusher (krad_compositor_t *krad_compositor, krad_display_api_t pusher) {
	if (pusher == 0) {
		krad_compositor_unset_pusher (krad_compositor);
	} else {
		if (krad_compositor->ticker_running == 1) {
			krad_compositor_stop_ticker (krad_compositor);
		}	
		krad_compositor->pusher = pusher;
	}
}

int krad_compositor_has_pusher (krad_compositor_t *krad_compositor) {
	if (krad_compositor->pusher == 0) {
		return FALSE;
	} else {
		return TRUE;
	}
}

krad_display_api_t krad_compositor_get_pusher (krad_compositor_t *krad_compositor) {
	return krad_compositor->pusher;
}

void krad_compositor_set_frame_rate (krad_compositor_t *krad_compositor,
									 int frame_rate_numerator, int frame_rate_denominator) {

	krad_compositor->frame_rate_numerator = frame_rate_numerator;
	krad_compositor->frame_rate_denominator = frame_rate_denominator;	
		
	if (krad_compositor->ticker_running == 1) {
		krad_compositor_stop_ticker (krad_compositor);
		krad_compositor->frame_num = 0;
		krad_compositor_start_ticker (krad_compositor);
	} else {
		krad_compositor->frame_num = 0;	
	}
	
}

void krad_compositor_set_dir (krad_compositor_t *krad_compositor, char *dir) {

	if (krad_compositor->dir != NULL) {
		free (krad_compositor->dir);
	}

	krad_compositor->dir = strdup (dir);

}

void krad_compositor_set_peak (krad_compositor_t *krad_compositor, int channel, float value) {
	
	int c;
	int fps;
	float kick;
	
	kick = 0.0;
	
	c = channel;
	fps = 30;

	if (value >= krad_compositor->krad_gui->output_peak[c]) {
		if (value > 2.7f) {
			krad_compositor->krad_gui->output_peak[c] = value;
			kick = 
			((krad_compositor->krad_gui->output_peak[channel] - krad_compositor->krad_gui->output_current[c]) / 300.0);
		}
	} else {
		if (krad_compositor->krad_gui->output_peak[c] == krad_compositor->krad_gui->output_current[c]) {
			krad_compositor->krad_gui->output_peak[c] -= (0.9 * (60/fps));
			if (krad_compositor->krad_gui->output_peak[c] < 0.0f) {
				krad_compositor->krad_gui->output_peak[c] = 0.0f;
			}
			krad_compositor->krad_gui->output_current[c] = krad_compositor->krad_gui->output_peak[c];
		}
	}

	if (krad_compositor->krad_gui->output_peak[c] > krad_compositor->krad_gui->output_current[c]) {
		krad_compositor->krad_gui->output_current[c] = 
		(krad_compositor->krad_gui->output_current[c] + 1.4) * (1.3 + kick);
	}

	if (krad_compositor->krad_gui->output_peak[c] < krad_compositor->krad_gui->output_current[c]) {
		krad_compositor->krad_gui->output_current[c] = krad_compositor->krad_gui->output_peak[c];
	}
}

void krad_compositor_port_to_ebml ( krad_ipc_server_t *krad_ipc, krad_compositor_port_t *krad_compositor_port) {

	uint64_t port;

	krad_ebml_start_element (krad_ipc->current_client->krad_ebml2, EBML_ID_KRAD_COMPOSITOR_PORT, &port);	

	krad_ipc_server_respond_number ( krad_ipc,
									 EBML_ID_KRAD_COMPOSITOR_PORT_SOURCE_WIDTH,
									 krad_compositor_port->source_width);
									 
	krad_ebml_finish_element (krad_ipc->current_client->krad_ebml2, port);

}

int krad_compositor_handler ( krad_compositor_t *krad_compositor, krad_ipc_server_t *krad_ipc ) {


	uint32_t ebml_id;
	
	uint32_t command;
	uint64_t ebml_data_size;
	uint64_t element;
	uint64_t response;	
	
	uint64_t numbers[32];		
	float floats[32];			
	
	char string[1024];
	char string2[1024];	
	
	int p;
	
	p = 0;
	string[0] = '\0';
	string2[0] = '\0';

	krad_ipc_server_read_command ( krad_ipc, &command, &ebml_data_size);

	switch ( command ) {
	
		case EBML_ID_KRAD_COMPOSITOR_CMD_UPDATE_PORT:

			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_PORT_NUMBER) {
				numbers[0] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}

			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_PORT_X) {
				numbers[1] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_PORT_Y) {
				numbers[2] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_PORT_WIDTH) {
				numbers[3] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_PORT_HEIGHT) {
				numbers[4] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_PORT_OPACITY) {
				floats[0] = krad_ebml_read_float (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_PORT_ROTATION) {
				floats[1] = krad_ebml_read_float (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}			
			
			
			krad_compositor_port_set_comp_params (&krad_compositor->port[numbers[0]],
										   		  numbers[3], numbers[4], numbers[1], numbers[2],
												  numbers[3],
												  numbers[4],
												  krad_compositor->port[numbers[0]].crop_x,
												  krad_compositor->port[numbers[0]].crop_y,
												  floats[0],
												  floats[1]);

			break;		
	
		case EBML_ID_KRAD_COMPOSITOR_CMD_INFO:
			sprintf (string, "Compositor Resolution: %d x %d Frame Rate: %d / %d - %f",
					 krad_compositor->width, krad_compositor->height,
					 krad_compositor->frame_rate_numerator, krad_compositor->frame_rate_denominator,
					 ((float)krad_compositor->frame_rate_numerator / (float)krad_compositor->frame_rate_denominator));
			krad_ipc_server_response_start ( krad_ipc, EBML_ID_KRAD_COMPOSITOR_MSG, &response);
			krad_ipc_server_respond_string ( krad_ipc, EBML_ID_KRAD_COMPOSITOR_INFO, string);
			krad_ipc_server_response_finish ( krad_ipc, response);
		
			break;

		case EBML_ID_KRAD_COMPOSITOR_CMD_SNAPSHOT:
			krad_compositor->snapshot++;
			break;
			
		case EBML_ID_KRAD_COMPOSITOR_CMD_SET_FRAME_RATE:

			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_FPS_NUMERATOR) {
				numbers[0] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}

			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_FPS_DENOMINATOR) {
				numbers[1] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_compositor_set_frame_rate (krad_compositor, numbers[0], numbers[1]);

			break;

		case EBML_ID_KRAD_COMPOSITOR_CMD_SET_RESOLUTION:

			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_WIDTH) {
				numbers[0] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}

			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_HEIGHT) {
				numbers[1] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_compositor_update_resolution (krad_compositor, numbers[0], numbers[1]);

			break;

	
		case EBML_ID_KRAD_COMPOSITOR_CMD_LIST_PORTS:

			
			krad_ipc_server_response_start ( krad_ipc, EBML_ID_KRAD_COMPOSITOR_MSG, &response);
			krad_ipc_server_response_list_start ( krad_ipc, EBML_ID_KRAD_COMPOSITOR_PORT_LIST, &element);	
			
			for (p = 0; p < KRAD_COMPOSITOR_MAX_PORTS; p++) {
				if (krad_compositor->port[p].active == 1) {
					//printf("Link %d Active: %s\n", k, krad_linker->krad_link[k]->mount);
					krad_compositor_port_to_ebml ( krad_ipc, &krad_compositor->port[p]);
				}
			}
			
			krad_ipc_server_response_list_finish ( krad_ipc, element );
			krad_ipc_server_response_finish ( krad_ipc, response );	
				
			break;	

		case  EBML_ID_KRAD_COMPOSITOR_CMD_VU_MODE:

			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);	

			if (ebml_id != EBML_ID_KRAD_COMPOSITOR_VU_ON) {
				//printf("hrm wtf3vu\n");
			} else {
				//printf("tag value size %zu\n", ebml_data_size);
			}

			krad_compositor->render_vu_meters = krad_ebml_read_number (krad_ipc->current_client->krad_ebml,
																	   ebml_data_size);

			break;

	
		case EBML_ID_KRAD_COMPOSITOR_CMD_SET_BUG:

			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);	

			if (ebml_id != EBML_ID_KRAD_COMPOSITOR_X) {
				//printf("hrm wtf3\n");
			} else {
				//printf("tag value size %zu\n", ebml_data_size);
			}

			krad_compositor->bug_x = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);

			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);	

			if (ebml_id != EBML_ID_KRAD_COMPOSITOR_Y) {
				//printf("hrm wtf3\n");
			} else {
				//printf("tag value size %zu\n", ebml_data_size);
			}

			krad_compositor->bug_y = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);	

			if (ebml_id != EBML_ID_KRAD_COMPOSITOR_FILENAME) {
				//printf("hrm wtf3\n");
			} else {
				//printf("tag value size %zu\n", ebml_data_size);
			}


			krad_ebml_read_string (krad_ipc->current_client->krad_ebml, string, ebml_data_size);

			krad_compositor->bug_filename = strdup (string);

			/*
			krad_ipc_server_response_start ( krad_ipc, EBML_ID_KRAD_LINK_MSG, &response);
			krad_ipc_server_response_list_start ( krad_ipc, EBML_ID_KRAD_LINK_LINK_LIST, &element);	
			
			for (k = 0; k < KRAD_LINKER_MAX_LINKS; k++) {
				if (krad_linker->krad_link[k] != NULL) {
					//printf("Link %d Active: %s\n", k, krad_linker->krad_link[k]->mount);
					krad_linker_link_to_ebml ( krad_ipc, krad_linker->krad_link[k]);
				}
			}
			
			krad_ipc_server_response_list_finish ( krad_ipc, element );
			krad_ipc_server_response_finish ( krad_ipc, response );	
			*/		
			break;
			
		case EBML_ID_KRAD_COMPOSITOR_CMD_SET_BACKGROUND:
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);	

			if (ebml_id != EBML_ID_KRAD_COMPOSITOR_FILENAME) {
				//printf("hrm wtf3\n");
			} else {
				//printf("tag value size %zu\n", ebml_data_size);
			}

			krad_ebml_read_string (krad_ipc->current_client->krad_ebml, string, ebml_data_size);

			krad_compositor_set_background (krad_compositor, string);
		
			break;			
	
		case EBML_ID_KRAD_COMPOSITOR_CMD_CLOSE_DISPLAY:
		
			krad_compositor_close_display (krad_compositor);		
			
			break;
		case EBML_ID_KRAD_COMPOSITOR_CMD_OPEN_DISPLAY:
			
			krad_compositor->display_width = krad_compositor->width;
			krad_compositor->display_height = krad_compositor->height;
			
			krad_compositor_open_display (krad_compositor);
			
			break;		

		case EBML_ID_KRAD_COMPOSITOR_CMD_ADD_SPRITE:
		
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);	

			if (ebml_id != EBML_ID_KRAD_COMPOSITOR_FILENAME) {
				//printf("hrm wtf3\n");
			} else {
				//printf("tag value size %zu\n", ebml_data_size);
			}

			krad_ebml_read_string (krad_ipc->current_client->krad_ebml, string, ebml_data_size);
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_X) {
				numbers[0] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_Y) {
				numbers[1] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_SPRITE_TICKRATE) {
				numbers[2] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_SPRITE_SCALE) {
				floats[0] = krad_ebml_read_float (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_SPRITE_OPACITY) {
				floats[1] = krad_ebml_read_float (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_SPRITE_ROTATION) {
				floats[2] = krad_ebml_read_float (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}							
			
			krad_compositor_add_sprite (krad_compositor, string, numbers[0], numbers[1], numbers[2], 
										floats[0], floats[1], floats[2]);
		
			break;

		case EBML_ID_KRAD_COMPOSITOR_CMD_SET_SPRITE:
	
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_SPRITE_NUMBER) {
				numbers[0] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}	
		
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_X) {
				numbers[1] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_Y) {
				numbers[2] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_SPRITE_TICKRATE) {
				numbers[3] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_SPRITE_SCALE) {
				floats[0] = krad_ebml_read_float (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_SPRITE_OPACITY) {
				floats[1] = krad_ebml_read_float (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_SPRITE_ROTATION) {
				floats[2] = krad_ebml_read_float (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}							
			
			krad_compositor_set_sprite (krad_compositor, numbers[0], numbers[1], numbers[2], numbers[3],
										floats[0], floats[1], floats[2]);		
		
			break;
			
		case EBML_ID_KRAD_COMPOSITOR_CMD_REMOVE_SPRITE:
		
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_SPRITE_NUMBER) {
				numbers[0] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
		
			krad_compositor_remove_sprite (krad_compositor, numbers[0]);
		
			break;
			
		case EBML_ID_KRAD_COMPOSITOR_CMD_LIST_SPRITES:
		
		
			break;						

		case EBML_ID_KRAD_COMPOSITOR_CMD_ADD_TEXT:
		
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);	

			if (ebml_id != EBML_ID_KRAD_COMPOSITOR_TEXT) {
				//printf("hrm wtf3\n");
			} else {
				//printf("tag value size %zu\n", ebml_data_size);
			}

			krad_ebml_read_string (krad_ipc->current_client->krad_ebml, string, ebml_data_size);
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_X) {
				numbers[0] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_Y) {
				numbers[1] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_TEXT_TICKRATE) {
				numbers[2] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_TEXT_SCALE) {
				floats[0] = krad_ebml_read_float (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_TEXT_OPACITY) {
				floats[1] = krad_ebml_read_float (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_TEXT_ROTATION) {
				floats[2] = krad_ebml_read_float (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_RED) {
				numbers[3] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_GREEN) {
				numbers[4] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_BLUE) {
				numbers[5] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);	

			if (ebml_id != EBML_ID_KRAD_COMPOSITOR_FONT) {
				//printf("hrm wtf3\n");
			} else {
				//printf("tag value size %zu\n", ebml_data_size);
			}

			krad_ebml_read_string (krad_ipc->current_client->krad_ebml, string2, ebml_data_size);					
			
			krad_compositor_add_text (krad_compositor, string, numbers[0], numbers[1], numbers[2], 
										floats[0], floats[1], floats[2], numbers[3], numbers[4], numbers[5], string2);
		
			break;

		case EBML_ID_KRAD_COMPOSITOR_CMD_SET_TEXT:
	
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_TEXT_NUMBER) {
				numbers[0] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}	
		
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_X) {
				numbers[1] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_Y) {
				numbers[2] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_TEXT_TICKRATE) {
				numbers[3] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_TEXT_SCALE) {
				floats[0] = krad_ebml_read_float (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_TEXT_OPACITY) {
				floats[1] = krad_ebml_read_float (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_TEXT_ROTATION) {
				floats[2] = krad_ebml_read_float (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_RED) {
				numbers[4] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_GREEN) {
				numbers[5] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_BLUE) {
				numbers[6] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}					
			
			krad_compositor_set_text (krad_compositor, numbers[0], numbers[1], numbers[2], numbers[3],
										floats[0], floats[1], floats[2], numbers[4], numbers[5], numbers[6]);		
		
			break;
			
		case EBML_ID_KRAD_COMPOSITOR_CMD_REMOVE_TEXT:
		
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);
			if (ebml_id == EBML_ID_KRAD_COMPOSITOR_TEXT_NUMBER) {
				numbers[0] = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			}
		
			krad_compositor_remove_text (krad_compositor, numbers[0]);
		
			break;
			
		case EBML_ID_KRAD_COMPOSITOR_CMD_LIST_TEXTS:
		
		
			break;

		case EBML_ID_KRAD_COMPOSITOR_CMD_HEX_DEMO:

			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);	

			if (ebml_id != EBML_ID_KRAD_COMPOSITOR_X) {
				//printf("hrm wtf3\n");
			} else {
				//printf("tag value size %zu\n", ebml_data_size);
			}

			krad_compositor->hex_x = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);

			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);	

			if (ebml_id != EBML_ID_KRAD_COMPOSITOR_Y) {
				//printf("hrm wtf3\n");
			} else {
				//printf("tag value size %zu\n", ebml_data_size);
			}

			krad_compositor->hex_y = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);
			
			krad_ebml_read_element (krad_ipc->current_client->krad_ebml, &ebml_id, &ebml_data_size);	

			if (ebml_id != EBML_ID_KRAD_COMPOSITOR_SIZE) {
				//printf("hrm wtf3\n");
			} else {
				//printf("tag value size %zu\n", ebml_data_size);
			}

			krad_compositor->hex_size = krad_ebml_read_number (krad_ipc->current_client->krad_ebml, ebml_data_size);			

			/*
			krad_ipc_server_response_start ( krad_ipc, EBML_ID_KRAD_LINK_MSG, &response);
			krad_ipc_server_response_list_start ( krad_ipc, EBML_ID_KRAD_LINK_LINK_LIST, &element);	
			
			for (k = 0; k < KRAD_LINKER_MAX_LINKS; k++) {
				if (krad_linker->krad_link[k] != NULL) {
					//printf("Link %d Active: %s\n", k, krad_linker->krad_link[k]->mount);
					krad_linker_link_to_ebml ( krad_ipc, krad_linker->krad_link[k]);
				}
			}
			
			krad_ipc_server_response_list_finish ( krad_ipc, element );
			krad_ipc_server_response_finish ( krad_ipc, response );	
			*/		
			break;
	}

	return 0;
}

