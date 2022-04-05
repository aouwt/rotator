#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/XInput.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <ieee754.h>

//typedef double TFLOAT;
union TFLOAT {
	float scalar;
	long padding;
};

namespace config {
	// IMPORTANT!!!! List the INPUT devices which you want to rotate here (touchpad, touchscreen, etc)
	static const char *XDevs [] = {
		"MSFT0001:01 06CB:7F27 Touchpad",
		"Wacom HID 517E Pen stylus",
		"Wacom HID 517E Finger touch",
		"Wacom HID 517E Pen eraser"
	};
	
	// https://wiki.ubuntu.com/X/InputCoordinateTransformation
	// i have to admit, i did actually just transcribe these from the page
	// i dont fully understand the scheme :(
	static const TFLOAT Transform [4] [9] = {
		{ // NORMAL
			1, 0, 0,
			0, 1, 0,
			0, 0, 1
		}, { // LEFT
			0, -1, 1,
			1,  0, 0,
			0,  0, 1
		}, { // DOWN
			-1,  0, 1,
			 0, -1, 1,
			 0,  0, 1
		}, { // RIGHT
			 0, 1, 0,
			-1, 0, 1,
			 0, 0, 1
		}
	};
}


#define TDEVS (sizeof (config::XDevs) / sizeof (config::XDevs [0]))
namespace all {
	static void setup (void);
	static void enable (void);
	static void disable (void);
}
static bool on = true;


namespace x11 {
	static Display *disp;

	namespace rot {
		int default_screen;
		Window root;
		
		static void setup (void) {
			default_screen = DefaultScreen (disp);
			root = RootWindow (disp, default_screen);
		}
		
		static void set (Rotation dir) {
			XSync (disp, false);
			
			XRRScreenConfiguration *config = XRRGetScreenInfo (disp, root);
			
			Rotation currot;
			int sz = XRRConfigCurrentConfiguration (config, &currot);
			
			XRRSetScreenConfig (disp, config, root, sz, dir, CurrentTime);
		
			XRRFreeScreenConfigInfo (config);
		}
	}
	
	namespace touch {
		static XDevice *tdev [TDEVS] = { NULL };
		static Atom transform_property;
		static Atom float_type;
		
		static void setup (void) {
			int devices_n;
			XDeviceInfo *devices = XListInputDevices (disp, &devices_n);
			
			for (int i = 0; i != devices_n; i ++) {
				for (int j = 0; j != TDEVS; j ++) {
					if (!strcmp (devices [i].name, config::XDevs [j])) {
						tdev [j] = XOpenDevice (disp, devices [i].id);
						if (tdev [j] == NULL)
							fprintf (stderr, "Can't open device %s\n", config::XDevs [j]);
						else
							fprintf (stderr, "Opened device %s\n", config::XDevs [j]);
					}
				}
			}
			
			float_type = XInternAtom (disp, "FLOAT", false);
			transform_property = XInternAtom (disp, "Coordinate Transformation Matrix", false);
			
			XFreeDeviceList (devices);
		}
		
		static void set (Rotation dir) {
			
			const TFLOAT *trans; // 🏳️‍⚧️
			switch (dir) {
				case RR_Rotate_0: trans = &config::Transform [0] [0];
					break;
				case RR_Rotate_90: trans = &config::Transform [1] [0];
					break;
				case RR_Rotate_180: trans = &config::Transform [2] [0];
					break;
				case RR_Rotate_270: trans = &config::Transform [3] [0];
					break;
				default: exit (127);
					break;
			}
			
			for (int i = 0; i != TDEVS; i ++) {
				if (tdev [i] != NULL) {
					XChangeDeviceProperty (
						disp, tdev [i],
						transform_property, float_type, 32,
						PropModeReplace,
						(const unsigned char *) trans, 9
					);
				}
			}
			
		}
	}
	
	static void enable (void) { return; }
	static void disable (void) { return; }
	static void setup (void) {
		disp = XOpenDisplay (getenv ("DISPLAY"));
		rot::setup (); touch::setup ();
		XSync (disp, false);
	}
	static void set (Rotation dir) {
		rot::set (dir);
		touch::set (dir);
		XSync (disp, false);
	}
}


namespace motion {
	static GDBusProxy *proxy;
	
	
	namespace callback {
		static void update (
			GDBusProxy *proxy,
			GVariant *changed, GStrv invalidated,
			gpointer data
		) {
			GVariantDict dict;
			g_variant_dict_init (&dict, changed);
			
			if (g_variant_dict_contains (&dict, "AccelerometerOrientation")) {
				GVariant *variant = g_dbus_proxy_get_cached_property (proxy, "AccelerometerOrientation");
				
				const gchar *dir = g_variant_get_string (variant, NULL);
				
				Rotation rot;
				switch (dir[0]) {
					case 'n': //normal
						rot = RR_Rotate_0;
						break;
						
					case 'l': //left-up
						rot = RR_Rotate_90;
						break;
						
					case 'b': //bottom-up
						rot = RR_Rotate_0;
						break;
						
					case 'r': //right-up
						rot = RR_Rotate_270;
						break;
						
					default:
						exit (128);
						break;
				}
				x11::set (rot);
				
				g_variant_unref (variant);
			}
		}
	}
	
	static void enable (void) {
		proxy = g_dbus_proxy_new_for_bus_sync (
			G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE,
			NULL,
			"net.hadess.SensorProxy", "/net/hadess/SensorProxy",
			"net.hadess.SensorProxy",
			NULL, NULL
		);
		
		g_signal_connect (
			G_OBJECT (proxy), "g-properties-changed",
			G_CALLBACK (callback::update), NULL
		);
		
		g_dbus_proxy_call_sync (
			proxy,
			"ClaimAccelerometer",
			NULL, G_DBUS_CALL_FLAGS_NONE,
			-1, NULL, NULL
		);
	}
	
	static void disable (void) {
		g_object_unref (proxy); proxy = NULL;
	}
	
	static void setup (void) { return; }
}

namespace icon {
	static GtkStatusIcon *icon;
	
	namespace callback {
		static void onclick (GtkStatusIcon *self, GdkEventButton event, gpointer dat) {
			if (on) {
				on = false;
				all::disable ();
			} else {
				on = true;
				all::enable ();
			}
		}
	}
	
	static void enable (void) {
		gtk_status_icon_set_from_icon_name (icon, "changes-allow");
		gtk_status_icon_set_tooltip_text (icon, "Rotation unlocked");
	}
	
	static void disable (void) {
		gtk_status_icon_set_from_icon_name (icon, "changes-prevent");
		gtk_status_icon_set_tooltip_text (icon, "Rotation locked");
	}
	
	
	static void setup (void) {
		icon = gtk_status_icon_new ();
		gtk_status_icon_set_title (icon, "Rotation control");
		
		g_signal_connect (
			G_OBJECT (icon), "button-press-event",
			G_CALLBACK (callback::onclick), NULL
		);
		
		gtk_status_icon_set_visible (icon, TRUE);
	}
}

static void all::setup (void) {
	icon::setup ();
	motion::setup ();
	x11::setup ();
}

static void all::enable (void) {
	icon::enable ();
	motion::enable ();
	x11::enable ();
}

static void all::disable (void) {
	icon::disable ();
	motion::disable ();
	x11::disable ();
}

int main (int argc, char *argv []) {
	if (sizeof (float) != 4)
		puts ("WARN: sizeof (float) != 4");
	
	gtk_init (&argc, &argv);
	all::setup ();
	all::enable ();
	
	GMainLoop *loop = g_main_loop_new (NULL, TRUE);
	g_main_loop_run (loop);
	return 0;
}

