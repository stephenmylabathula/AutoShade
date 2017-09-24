/*************************************
 * File:   AutoShade.c
 * Author: Stephen Mylabathula
 *
 * Created on May 1, 2017, 11:33 PM
 * 
 * Dependencies: GTK2.0, GPSD, and SPA
 *************************************/

#include <gtk/gtk.h>
#include <gps.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include "spa.h"

/***** DEFINITIONS *****/
#define PHI1 30     //Driver Left FOV Angle (degrees)
#define PHI2 60     //Driver Right FOV Angle (degrees)
#define GAMMA 70    //Driver Max Zenith FOV (field of view)
#define MAX_ZENITH 90	//Driver Lowest Zenith Angle FOV. It is impossible to see the sun when it is below the horizon at 90 degrees. Therefore it is recommended to leave this parameter unchanged.
#define TMZN_OFFSET 0 //Timezone Offset from GMT

/***** STRUCTURE DEFINITIONS *****/
struct GPSData {
    float latitude;
    float longitude;
    float altitude;
    float heading;
};


/***** GLOBALS *****/
int spot_x = 0;     //X-Location of Black Spot
int spot_y = 0;     //Y-location of Black Spot
int needSpot = 1;   //1 - if we need to display the spot; 0 - otherwise
static GdkPixmap *pixmap = NULL; //Drawing Surface

int updateSPA = 60; //Update Interval: Only update every 60 seconds when updateSPA >= 60
spa_data spa;       //Stores current SPA Data
int result; //SPA Variable

time_t now;
struct tm *now_tm; //Time Variables

struct GPSData* currentGPSData;     //Stores current GPS Data
int rc;
struct timeval tv;
struct gps_data_t gps_dat; //GPS Variables


/***** METHODS *****/
/**
 * This function polls the GPS Module for data ad stores it if available.
 * TODO: Sometimes I notice that this function might not be reading all the values from the GPS buffer fast enough.
 *       Maybe there is a better way to clear data from th buffer rather than reading at a higher rate.
 */
void pollGPS() {
    if (gps_waiting(&gps_dat, 0)) {
        if ((rc = gps_read(&gps_dat)) == -1) {
            printf("GPS Data Reading Error: %d, reason: %s\n", rc, gps_errstr(rc));
        } else if (gps_dat.status == STATUS_FIX && gps_dat.fix.mode == MODE_3D && !isnan(gps_dat.fix.latitude) && !isnan(gps_dat.fix.longitude) && !isnan(gps_dat.fix.track) && !isnan(gps_dat.fix.altitude)) {
            //If we have a 3D fix (latitude, longitude, altitude) and a heading angle then store it into currentGPSData
            struct GPSData* temp = (struct GPSData*) malloc(sizeof(struct GPSData));
            temp->latitude = gps_dat.fix.latitude;
            temp->longitude = gps_dat.fix.longitude;
            temp->altitude = gps_dat.fix.altitude;
            temp->heading = gps_dat.fix.track;
	    currentGPSData = temp;
        } else {
            printf("GPS Data Unavailable\n");
        }
    }
}

/**
 * Determines y given x on the line between (x1, y1) and (x2, y2)
 * @param x1
 * @param y1
 * @param x2
 * @param y2
 * @param x
 * @return 
 */
int map(int x1, int y1, int x2, int y2, int x) {
    float m = (y2 - y1) / (x2 - x1);
    float b = y1 - m*x1;
    return (int) (m * x + b);
}

/***** CALLBACKS *****/
static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer data) {
    g_print("Will Exit!\n");
    return FALSE; //Allow Exit on Close
}

static void destroy(GtkWidget *widget, gpointer data) {
    gtk_main_quit();
}

/***** DRAWING METHODS/CALLBACKS *****/
/**
 * This function initially draws a white canvas. See expose_event().
 * @param widget
 * @param event
 * @return 
 */
static gboolean configure_event(GtkWidget *widget, GdkEventConfigure *event) {
    if (pixmap)
        g_object_unref(pixmap);

    pixmap = gdk_pixmap_new(widget->window, widget->allocation.width, widget->allocation.height, -1);
    gdk_draw_rectangle(pixmap, widget->style->white_gc, TRUE, 0, 0, widget->allocation.width, widget->allocation.height);

    if (needSpot)
        gdk_draw_arc(pixmap, widget->style->black_gc, TRUE, spot_x, spot_y, 100, 100, 0, 23040);    //Draw a circle at spot_x,spot_y that is 100px*100px

    return TRUE;
}

/**
 * This function draws a white canvas and draws a black spot (100px by 100px) if needed.
 * This function is called every time we repaint the canvas
 * @param widget
 * @param event
 * @return 
 */
static gboolean expose_event(GtkWidget *widget, GdkEventExpose *event) {
    gdk_draw_rectangle(pixmap, widget->style->white_gc, TRUE, 0, 0, widget->allocation.width, widget->allocation.height);
    if (needSpot)
        gdk_draw_arc(pixmap, widget->style->black_gc, TRUE, spot_x, spot_y, 100, 100, 0, 23040);    //Draw a circle at spot_x,spot_y that is 100px*100px

    gdk_draw_drawable(widget->window,
            widget->style->fg_gc[gtk_widget_get_state(widget)],
            pixmap,
            event->area.x, event->area.y,
            event->area.x, event->area.y,
            event->area.width, event->area.height);

    return FALSE;
}

/**
 * This function should be setup in main() to run every 500ms. 
 * It polls the GPS for data, and determines whether or not to display the black spot and if so, where to display it.
 * @param widget
 * @return 
 */
gboolean time_handler(GtkWidget *widget) {
    if (widget->window == NULL)
        return FALSE;

    updateSPA++;    //Increment SPA time counter

    //Update GPS Metrics
    pollGPS();
    
    if (currentGPSData) {
        //Calculate new SPA if a minute has elapsed
        if (updateSPA >= 60) {
            now = time(NULL);
            now_tm = localtime(&now);
            spa.hour = now_tm->tm_hour;
            spa.minute = now_tm->tm_min;
            spa.second = now_tm->tm_sec;
            spa.longitude = currentGPSData->longitude;
            spa.latitude = currentGPSData->latitude;
            spa.elevation = currentGPSData->altitude;
            result = spa_calculate(&spa);
            printf("NEW Zenith:    %.6f degrees   Azimuth:    %.6f degrees\n", spa.zenith, spa.azimuth);    //Prints the current Solar Zenith and Azimuth
            updateSPA = 0;  //Reset to Zero so we don't update SPA for another minute
        }

        if (!result) {  //if we have good SPA data then determine whether we need a black spot
            printf("Heading: %f\n", currentGPSData->heading);   //Print Heading
	    printf("OLD Zenith:    %.6f degrees   Azimuth:    %.6f degrees\n", spa.zenith, spa.azimuth);    //Prints the current Solar Zenith and Azimuth
            if (spa.azimuth >= currentGPSData->heading - PHI1 && spa.azimuth < currentGPSData->heading + PHI2 && spa.zenith >= GAMMA && spa.zenith < MAX_ZENITH) {
                needSpot = 1;

                //Update spot_x, spot_y
                spot_x = map(currentGPSData->heading - PHI1, 0, currentGPSData->heading + PHI2, widget->allocation.width, spa.azimuth);
                spot_y = map(GAMMA, 0, MAX_ZENITH, widget->allocation.height, spa.zenith);
            } else {
                needSpot = 0;
            }

	    //Repaint Canvas
            gtk_widget_queue_draw(widget);
        }
    }

    return TRUE;
}

int main(int argc, char *argv[]) {
    
    if(argc == 4){      //If the user has provided arguments, then run in test mode
        int a = atoi(argv[1]);  //input azimuth
        int z = atoi(argv[2]);  //input zenith
        int h = atoi(argv[3]);  //input heading

        if (a >= h - PHI1 && a < h + PHI2 && z >= GAMMA && z < MAX_ZENITH){     //if the sun is visible to the driver, then we need to display a black spot
            needSpot = 1;
            spot_x = map(h - PHI1, 0, h + PHI2, 1072, a);
            spot_y = map(GAMMA, 0, MAX_ZENITH, 704, z);
        } else {
            needSpot = 0;
        }
    }

    //Initialize GPS
    if ((rc = gps_open("localhost", "2947", &gps_dat)) == -1) {
        printf("code: %d, reason: %s\n", rc, gps_errstr(rc));
        return EXIT_FAILURE;
    }
    gps_stream(&gps_dat, WATCH_ENABLE | WATCH_JSON, NULL);

    //Initialize SPA Calculator
    now = time(NULL);
    now_tm = localtime(&now);
    spa.year = now_tm->tm_year;
    spa.month = now_tm->tm_mon;
    spa.day = now_tm->tm_mday;
    spa.timezone = 0 - TMZN_OFFSET; //IF PI USES GMT THIS IS 0
    spa.delta_ut1 = 0;
    spa.delta_t = 67;
    spa.pressure = 820;     //Annual Average Local Atmospheric Pressure (millibars)
    spa.temperature = 11;   //Annual Average Local Temperature (Celsius)
    spa.slope = 30;         //Surface slope measured from the horizontal plane (refer to pg.8 in NREL SPA)
    spa.azm_rotation = -10; //surface azimuth rotation angle, measured from south to the projection of the surface normal on the horizontal plane, positive or negative if oriented west or east from south, respectively. (refer to pg.11 in NREL SPA))
    spa.atmos_refract = 0.5667; //Atmosphereic Refraction Correction Angle (refer to pg.16 and pg.A-1 in NREL SPA)
    spa.function = SPA_ZA; //Only calculate and return Zenith and Azimuth data. If you want all SPA data use flag SPA_ALL.

    //Initialize GTK Drawing Widgets
    GtkWidget *window;
    GtkWidget *canvas;

    gtk_init(&argc, &argv); //Process Arguments

    //Initialize Top Window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_maximize((GtkWindow*) window);   //Set Window Maximized
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);

    g_signal_connect(window, "delete-event", G_CALLBACK(delete_event), NULL); //Connect Window Callbacks
    g_signal_connect(window, "destroy", G_CALLBACK(destroy), NULL);

    //Initialize Canvas Area
    canvas = gtk_drawing_area_new();
    g_signal_connect(canvas, "configure_event", G_CALLBACK(configure_event), NULL);
    g_signal_connect(canvas, "expose_event", G_CALLBACK(expose_event), NULL); //Connect Canvas Callbacks

    //Setup Timer
    g_timeout_add(500, (GSourceFunc) time_handler, (gpointer) window);
    time_handler(canvas);   //Run the time_handler function every 500ms

    //Pack and Show Components
    gtk_container_add(GTK_CONTAINER(window), canvas);
    gtk_widget_show(canvas);
    gtk_widget_show(window);

    //Run App
    gtk_main();

    //Close GPS Stream
    gps_stream(&gps_dat, WATCH_DISABLE, NULL);
    gps_close(&gps_dat);

    return 0;
}
