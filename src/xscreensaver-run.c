#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <strings.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/prctl.h>
#include <X11/keysym.h>
#include <sys/wait.h>

#define MAX_LINE 1024
#define MAX_SAVERS 100
#define MAX_ARGS 100

static int g_verbose = 0;

#define verbose_printf(args...)    \
    do                             \
    {                              \
        if (g_verbose)             \
        {                          \
            fprintf(stderr, args); \
        }                          \
    } while (0)

typedef struct
{
    char *args[MAX_ARGS];
    int arg_count;
} Screensaver;

static Screensaver savers[MAX_SAVERS];
static int saver_count = 0;
static int current_saver = 0;

void usage()
{
    fprintf(stderr, "Usage: xscreensaver-run [-h] [-v] SCREENSAVER_FILE_LIST\n");
}

Cursor blankcursor(Display *dis, Window win)
{
    static char data[1] = {0};
    Cursor cursor;
    Pixmap blank;
    XColor dummy;

    blank = XCreateBitmapFromData(dis, win, data, 1, 1);
    if (blank == None)
    {
        verbose_printf("Failed to create a blank cursor for %ld\n", win);
        return 0;
    }
    cursor = XCreatePixmapCursor(dis, blank, blank, &dummy, &dummy, 0, 0);
    XFreePixmap(dis, blank);
    return cursor;
}

void load_screensavers(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        perror("Failed to open screensaver file");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), fp) && saver_count < MAX_SAVERS)
    {
        // Remove trailing newline if present
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
        {
            line[len - 1] = '\0';
        }

        // Skip empty lines
        if (strlen(line) == 0)
        {
            continue;
        }

        // Make a copy of the line since we'll modify it
        char *args_line = strdup(line);
        int arg_count = 0;
        char *token = strtok(args_line, " ");

        while (token && arg_count < MAX_ARGS - 3)
        {
            savers[saver_count].args[arg_count] = strdup(token);
            arg_count++;
            token = strtok(NULL, " ");
        }

        free(args_line);
        savers[saver_count].arg_count = arg_count;
        saver_count++;
    }
    fclose(fp);
}

pid_t spawn_screensaver(Window win)
{
    char buf[20];
    snprintf(buf, sizeof(buf), "0x%lx", win);

    Screensaver *current = &savers[current_saver];

    // Create a persistent buffer for the window ID
    current->args[current->arg_count] = strdup("-window-id");
    current->args[current->arg_count + 1] = strdup(buf);
    current->args[current->arg_count + 2] = NULL;

    // Print all arguments before fork
    verbose_printf("Executing command:");
    for (int i = 0; current->args[i] != NULL; i++)
    {
        verbose_printf(" %s", current->args[i]);
    }
    verbose_printf("\n");

    pid_t chld_id = fork();
    if (!chld_id)
    {
        execvp(current->args[0], current->args);
        verbose_printf("Failed to run the command '%s'\n", current->args[0]);
        exit(2);
    }

    // Free the temporary arguments after fork
    if (chld_id > 0)
    {
        free(current->args[current->arg_count]);
        free(current->args[current->arg_count + 1]);
    }

    return chld_id;
}

int main(int argc, char **argv)
{
    int opt;
    char *filename;

    while ((opt = getopt(argc, argv, "hv")) != -1)
    {
        switch (opt)
        {
        case 'h':
            usage();
            exit(EXIT_FAILURE);
        case 'v':
            g_verbose = 1;
            break;
        default:
            usage();
            exit(EXIT_FAILURE);
        }
    }

    // Check if there's exactly one positional argument
    if (optind >= argc)
    {
        fprintf(stderr, "Error: Screensaver file list is required\n");
        usage();
        exit(EXIT_FAILURE);
    }

    if (optind < argc - 1)
    {
        fprintf(stderr, "Error: Too many arguments\n");
        usage();
        exit(EXIT_FAILURE);
    }

    filename = argv[optind];

    load_screensavers(filename);
    if (saver_count == 0)
    {
        fprintf(stderr, "No screensavers loaded\n");
        exit(EXIT_FAILURE);
    }

    Display *dis = XOpenDisplay(NULL);
    Window win = XCreateSimpleWindow(dis, RootWindow(dis, 0), 0, 0, 10, 10,
                                     0, BlackPixel(dis, 0), BlackPixel(dis, 0));

    Atom wm_state = XInternAtom(dis, "_NET_WM_STATE", False);
    Atom fullscreen = XInternAtom(dis, "_NET_WM_STATE_FULLSCREEN", False);

    XEvent xev = {0};
    xev.type = ClientMessage;
    xev.xclient.window = win;
    xev.xclient.message_type = wm_state;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = 1;
    xev.xclient.data.l[1] = fullscreen;

    XMapWindow(dis, win);
    XSendEvent(dis, DefaultRootWindow(dis), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &xev);

    Cursor bc = blankcursor(dis, win);
    XDefineCursor(dis, win, bc);
    XFlush(dis);

    pid_t current_pid = spawn_screensaver(win);

    XGrabPointer(dis, win, True,
                 ButtonReleaseMask | ButtonPressMask | Button1MotionMask,
                 GrabModeAsync, GrabModeAsync,
                 win, bc, CurrentTime);

    XSelectInput(dis, win,
                 KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);

    while (1)
    {
        XEvent event;
        XAllowEvents(dis, SyncPointer, CurrentTime);
        XWindowEvent(dis, win,
                     ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask,
                     &event);

        if (event.type == KeyPress)
        {
            KeySym key = XLookupKeysym(&event.xkey, 0);
            if (key == XK_Escape)
            {
                kill(current_pid, SIGHUP);
                waitpid(current_pid, NULL, 0);

                current_saver = (current_saver + 1) % saver_count;
                current_pid = spawn_screensaver(win);
            }
        }
    }

    XUngrabPointer(dis, CurrentTime);
    XDestroyWindow(dis, win);
    XCloseDisplay(dis);
    return 0;
}
