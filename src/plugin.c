/* 
 * Wavelet denoise GIMP plugin
 * 
 * plugin.c
 * Copyright 2008 by Marco Rossini
 * 
 * Implements the wavelet denoise code of UFRaw by Udi Fuchs
 * which itself bases on the code by Dave Coffin
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 * 
 */

#include "plugin.h"
#include <string.h>		//*JT* (for memcpy)

GimpPlugInInfo PLUG_IN_INFO = { NULL, NULL, query, run };

wavelet_settings settings = {
  {0, 0},			/* gray_thresholds */
  {0.0, 0.0},			/* gray_low */
  {0, 0, 0, 0},			/* colour_thresholds */
  {0.0, 0.0, 0.0, 0.0},		/* colour_low */
  MODE_YCBCR,			/* colour_mode */
  0,				/* preview_channel */
  1,				/* preview_mode */
  TRUE,				/* preview */
  {1.1, 4.5, 1.4},		/* times */
  0, 0				/* winxsize, winysize */
};

char *names_ycbcr[] = { "Y", "Cb", "Cr", N_("Alpha") };
char *names_rgb[] = { "R", "G", "B", N_("Alpha") };
char *names_gray[] = { "Gray", N_("Alpha") };
char *names_lab[] = { "L*", "a*", "b*", N_("Alpha") };

MAIN ()
     void query (void)
{
  static GimpParamDef args[] = {
    /* TRANSLATORS: This is a plugin argument for scripts */
    {GIMP_PDB_INT32, "run-mode", "Run mode { RUN-INTERACTIVE (0), RUN-NONINTERACTIVE (1) }"},
    /* TRANSLATORS: This is a plugin argument for scripts */
    {GIMP_PDB_IMAGE, "image", "Input image"},
    /* TRANSLATORS: This is a plugin argument for scripts */
    {GIMP_PDB_DRAWABLE, "drawable", "Input drawable"},
//*JT* START
    {GIMP_PDB_INT32,      "colour-mode",	 "Colour mode { 0=YCBCR, 1=RGB, 2=LAB }"},
/* First attempt, using floatarrays ..too much trouble, better to have individual float values
    {GIMP_PDB_INT32,      "n-gray-thresholds",	 "Number of float values in following array"},
    {GIMP_PDB_FLOATARRAY, "gray-thresholds",	 "Array of up to 2 gray thresholds"},
    {GIMP_PDB_INT32,      "n-gray-low",		 "Number of float values in following array"},
    {GIMP_PDB_FLOATARRAY, "gray-low",		 "Array of up to 2 gray low values"},
    {GIMP_PDB_INT32,      "n-colour-thresholds", "Number of float values in following array"},
    {GIMP_PDB_FLOATARRAY, "colour-thresholds",	 "Array of up to 4 colour thresholds"},
    {GIMP_PDB_INT32,      "n-colour-low",	 "Number of float values in following array"},
    {GIMP_PDB_FLOATARRAY, "colour-low",		 "Array of up to 4 colour low values"}
*/
    {GIMP_PDB_FLOAT, "gray-threshold-0",	 "Gray channel 0 threshold {0.0-10.0}"},
    {GIMP_PDB_FLOAT, "gray-threshold-1",	 "Gray channel 1 threshold {0.0-10.0}"},
    {GIMP_PDB_FLOAT, "gray-low-0",		 "Gray channel 0 low (softness) {0.0-1.0}"},
    {GIMP_PDB_FLOAT, "gray-low-1",		 "Gray channel 1 low (softness) {0.0-1.0}"},
    {GIMP_PDB_FLOAT, "colour-threshold-0",	 "Colour channel 0 threshold {0.0-10.0}"},
    {GIMP_PDB_FLOAT, "colour-threshold-1",	 "Colour channel 1 threshold {0.0-10.0}"},
    {GIMP_PDB_FLOAT, "colour-threshold-2",	 "Colour channel 2 threshold {0.0-10.0}"},
    {GIMP_PDB_FLOAT, "colour-threshold-3",	 "Colour channel 3 threshold {0.0-10.0}"},
    {GIMP_PDB_FLOAT, "colour-low-0",		 "Colour channel 0 low (softness) {0.0-1.0}"},
    {GIMP_PDB_FLOAT, "colour-low-1",		 "Colour channel 1 low (softness) {0.0-1.0}"},
    {GIMP_PDB_FLOAT, "colour-low-2",		 "Colour channel 2 low (softness) {0.0-1.0}"},
    {GIMP_PDB_FLOAT, "colour-low-3",		 "Colour channel 3 low (softness) {0.0-1.0}"}
//*JT* END
  };

  gimp_install_procedure ("plug-in-wavelet-denoise",
			  _("Removes noise in the image using wavelets."),
			  PLUGIN_HELP,
			  _("Marco Rossini"),
			  _("Copyright 2008 Marco Rossini"),
			  "2008",
			  /* TRANSLATORS: Menu entry of the plugin. Use under-
			     score for identifying hotkey */
			  N_("_Wavelet denoise ..."),
			  "RGB*, GRAY*",
			  GIMP_PLUGIN, G_N_ELEMENTS (args), 0, args, NULL);

  gimp_plugin_domain_register("gimp20-wavelet-denoise-plug-in", LOCALEDIR);
  
  gimp_plugin_menu_register ("plug-in-wavelet-denoise",
			     "<Image>/Filters/Enhance");
}

void
run (const gchar * name, gint nparams, const GimpParam * param,
     gint * nreturn_vals, GimpParam ** return_vals)
{
  static GimpParam values[1];
  GimpRunMode run_mode;
  GimpDrawable *drawable;
  gint i;
  gboolean run_ok = TRUE;	//*JT*

  bindtextdomain("gimp20-wavelet-denoise-plug-in", LOCALEDIR);
  textdomain("gimp20-wavelet-denoise-plug-in");
  bind_textdomain_codeset("gimp20-wavelet-denoise-plug-in", "UTF-8");

  timer = g_timer_new ();

  /* Setting mandatory output values */
  *nreturn_vals = 1;
  *return_vals = values;
  values[0].type = GIMP_PDB_STATUS;
  values[0].data.d_status = GIMP_PDB_SUCCESS;

  /* restore settings saved in GIMP core */
  gimp_get_data ("plug-in-wavelet-denoise", &settings);

  drawable = gimp_drawable_get (param[2].data.d_drawable);
  channels = gimp_drawable_bpp (drawable->drawable_id);

  if (settings.preview_channel > channels - 1)
    settings.preview_channel = 0;

  /* allocate buffers */
  /* FIXME: replace by GIMP funcitons */
  for (i = 0; i < channels; i++)
    {
      fimg[i] = (float *) malloc (drawable->width * drawable->height
				  * sizeof (float));
    }
  buffer[1] = (float *) malloc (drawable->width * drawable->height
				* sizeof (float));
  buffer[2] = (float *) malloc (drawable->width * drawable->height
				* sizeof (float));

  /* run GUI if in interactiv mode */
  run_mode = param[0].data.d_int32;
  if (run_mode == GIMP_RUN_INTERACTIVE)
    {
      if (!user_interface (drawable))
	{
	  gimp_drawable_detach (drawable);
	  /* FIXME: should return error status here */
	  return;
	}
    }
  //*JT* START
  else if (run_mode == GIMP_RUN_NONINTERACTIVE)
    {
      gboolean range_check_thr( double val ) { return ((val >= 0.0) && (val <= 10.0)); }
      gboolean range_check_low( double val ) { return ((val >= 0.0) && (val <= 1.0)); }

      if ( (nparams != 16) ||
	   !range_check_thr( param[4].data.d_float ) ||
	   !range_check_thr( param[5].data.d_float ) ||
	   !range_check_low( param[6].data.d_float ) ||
	   !range_check_low( param[7].data.d_float ) ||
	   !range_check_thr( param[8].data.d_float ) ||
	   !range_check_thr( param[9].data.d_float ) ||
	   !range_check_thr( param[10].data.d_float ) ||
	   !range_check_thr( param[11].data.d_float ) ||
	   !range_check_low( param[12].data.d_float ) ||
	   !range_check_low( param[13].data.d_float ) ||
	   !range_check_low( param[14].data.d_float ) ||
	   !range_check_low( param[15].data.d_float )
	 )
	 {run_ok = FALSE; values[0].data.d_status = GIMP_PDB_CALLING_ERROR;}
      else
      {
	settings.colour_mode = param[3].data.d_int32;;
/*
	memcpy( &settings.gray_thresholds,   param[5].data.d_floatarray,  sizeof(double) * param[4].data.d_int32);
	memcpy( &settings.gray_low,          param[7].data.d_floatarray,  sizeof(double) * param[6].data.d_int32);
	memcpy( &settings.colour_thresholds, param[9].data.d_floatarray,  sizeof(double) * param[8].data.d_int32);
	memcpy( &settings.colour_low,        param[11].data.d_floatarray, sizeof(double) * param[10].data.d_int32);
*/
	settings.gray_thresholds[0]	= param[4].data.d_float;
	settings.gray_thresholds[1]	= param[5].data.d_float;
	settings.gray_low[0]		= param[6].data.d_float;
	settings.gray_low[1]		= param[7].data.d_float;
	settings.colour_thresholds[0]	= param[8].data.d_float;
	settings.colour_thresholds[1]	= param[9].data.d_float;
	settings.colour_thresholds[2]	= param[10].data.d_float;
	settings.colour_thresholds[3]	= param[11].data.d_float;
	settings.colour_low[0]		= param[12].data.d_float;
	settings.colour_low[1]		= param[13].data.d_float;
	settings.colour_low[2]		= param[14].data.d_float;
	settings.colour_low[3]		= param[15].data.d_float;
      }
    }

  if (run_ok)
    denoise (drawable, NULL);
  //*JT END

  /* free buffers */
  /* FIXME: replace by GIMP functions */
  for (i = 0; i < channels; i++)
    {
      free (fimg[i]);
    }
  free (buffer[1]);
  free (buffer[2]);

  gimp_displays_flush ();
  gimp_drawable_detach (drawable);

  /* save settings in the GIMP core */
  gimp_set_data ("plug-in-wavelet-denoise", &settings,
		 sizeof (wavelet_settings));
}
