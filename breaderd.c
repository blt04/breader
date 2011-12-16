#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <termios.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>


#define DEFAULT_UINPUT "/dev/input/uinput"
#define DEFAULT_BAUDRATE B9600
#define DEFAULT_PIDFILE "/var/run/breaderd.pid"

#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;


/* Globals */
static int uinp_fd = -1;
static int tty_fd  = -1;
struct uinput_user_dev uinp; // uInput device structure
struct input_event event; // Input device structure


const char asciiEvKey[128] = {
   -1, -1, -1, -1, -1, -1, -1, -1,  // 0x00
   KEY_BACKSPACE, KEY_TAB, KEY_ENTER, -1, -1, KEY_ENTER, -1, -1,  // 0x08
   -1, -1, -1, -1, -1, -1, -1, -1,  // 0x10
   -1, -1, -1, -1, -1, -1, -1, -1,  // 0x18
   KEY_SPACE, KEY_1, KEY_APOSTROPHE, KEY_3, KEY_4, KEY_5, KEY_7, KEY_APOSTROPHE,  // 0x20
   KEY_9, KEY_0, KEY_8, KEY_EQUAL, KEY_COMMA, KEY_MINUS, KEY_DOT, KEY_SLASH,  // 0x28
   KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7,  // 0x30
   KEY_8, KEY_9, KEY_SEMICOLON, KEY_SEMICOLON, KEY_COMMA, KEY_EQUAL, KEY_DOT, KEY_SLASH,  // 0x38
   KEY_2, KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G,  // 0x40
   KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O,  // 0x48
   KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W,  // 0x50
   KEY_X, KEY_Y, KEY_Z, KEY_LEFTBRACE, KEY_BACKSLASH, KEY_RIGHTBRACE, KEY_6, KEY_MINUS,  // 0x58
   KEY_GRAVE, KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G,  // 0x60
   KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O,  // 0x68
   KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W,  // 0x70
   KEY_X, KEY_Y, KEY_Z, KEY_LEFTBRACE, KEY_BACKSLASH, KEY_RIGHTBRACE, KEY_GRAVE, KEY_DELETE // 0x78
};

const char asciiShift[128] = {
    0,  0,  0,  0,  0,  0,  0,  0,  // 0x00
    0,  0,  0,  0,  0,  0,  0,  0,  // 0x08
    0,  0,  0,  0,  0,  0,  0,  0,  // 0x10
    0,  0,  0,  0,  0,  0,  0,  0,  // 0x18
    0, -1, -1, -1, -1, -1, -1,  0,  // 0x20
   -1, -1, -1, -1,  0,  0,  0,  0,  // 0x28
    0,  0,  0,  0,  0,  0,  0,  0,  // 0x30
    0,  0, -1,  0, -1,  0, -1, -1,  // 0x38
   -1, -1, -1, -1, -1, -1, -1, -1,  // 0x40
   -1, -1, -1, -1, -1, -1, -1, -1,  // 0x48
   -1, -1, -1, -1, -1, -1, -1, -1,  // 0x50
   -1, -1, -1,  0,  0,  0, -1, -1,  // 0x58
    0,  0,  0,  0,  0,  0,  0,  0,  // 0x60
    0,  0,  0,  0,  0,  0,  0,  0,  // 0x68
    0,  0,  0,  0,  0,  0,  0,  0,  // 0x70
    0,  0,  0, -1, -1, -1, -1,  0   // 0x78
};



/* Setup the uinput device */
int setup_uinput_device(char *uinputdev)
{
   int i=0;

   uinp_fd = open (uinputdev, O_WRONLY | O_NDELAY);
   if (uinp_fd == -1)
   {
      perror(uinputdev);
      fprintf(stderr, "Make sure the uinput module is loaded or\nCONFIG_INPUT_UINPUT is compiled into the kernel.\n");
      return -1;
   }

   memset(&uinp, 0, sizeof(uinp));
   strncpy(uinp.name, "Generic Serial Barcode Scanner", UINPUT_MAX_NAME_SIZE);
   uinp.id.version = 4;
   uinp.id.bustype = BUS_USB;

   ioctl(uinp_fd, UI_SET_EVBIT, EV_KEY);
   ioctl(uinp_fd, UI_SET_EVBIT, EV_REP);
   for (i=0; i < 256; i++) {
      ioctl(uinp_fd, UI_SET_KEYBIT, i);
   }

   write(uinp_fd, &uinp, sizeof(uinp));
   if (ioctl(uinp_fd, UI_DEV_CREATE))
   {
      printf("Unable to create UINPUT device.");
      return -1;
   }

   // Allow time for X to start listening to the new input
   sleep(1);

   return 0;
}

void destroy_uinput_device()
{
   ioctl(uinp_fd, UI_DEV_DESTROY);
   close(uinp_fd);
}


void pressEvKey(int evKey)
{
   // Report BUTTON CLICK - PRESS event
   memset(&event, 0, sizeof(event));
   gettimeofday(&event.time, NULL);
   event.type = EV_KEY;
   event.code = evKey;
   event.value = 1;
   write(uinp_fd, &event, sizeof(event));
   event.type = EV_SYN;
   event.code = SYN_REPORT;
   event.value = 0;
   write(uinp_fd, &event, sizeof(event));
}

void releaseEvKey(int evKey)
{
   // Report BUTTON CLICK - RELEASE event
   memset(&event, 0, sizeof(event));
   gettimeofday(&event.time, NULL);
   event.type = EV_KEY;
   event.code = evKey;
   event.value = 0;
   write(uinp_fd, &event, sizeof(event));
   event.type = EV_SYN;
   event.code = SYN_REPORT;
   event.value = 0;
   write(uinp_fd, &event, sizeof(event));
}


void sendEvKey(int evKey)
{
   pressEvKey(evKey);
   releaseEvKey(evKey);
}

void sendShiftEvKey(int evKey)
{
   pressEvKey(KEY_LEFTSHIFT);
   sendEvKey(evKey);
   releaseEvKey(KEY_LEFTSHIFT);
}



void sendAscii(char ascii)
{
   if (asciiEvKey[ascii]==-1) return;

   if (asciiShift[ascii])
      sendShiftEvKey(asciiEvKey[ascii]);
   else
      sendEvKey(asciiEvKey[ascii]);
   usleep(15000);
}


void sendString(char *str)
{
   int i=0;
   int len=strlen(str);
   for (i=0; i<len; i++)
   {
      // Skip carraige returns
      if (str[i] != 0x0d) sendAscii(str[i]);
   }
}



void listen(long baudrate)
{
   int c, res;
   struct termios oldtio,newtio;
   char buf[255];
   int prevlf = FALSE;


   tcgetattr(tty_fd, &oldtio); // save current modem settings

   bzero(&newtio, sizeof(newtio));

   /*
     Set bps rate and hardware flow control and 8n1 (8bit,no parity,1 stopbit).
     Also don't hangup automatically and ignore modem status.
     Finally enable receiving characters.
   */
   newtio.c_cflag = baudrate | CRTSCTS | CS8 | CLOCAL | CREAD;
   newtio.c_iflag = IGNPAR;
   newtio.c_oflag = 0;

   /*
     Don't echo characters.
   */
   newtio.c_lflag = 0;

   /* blocking read until 1 char arrives */
   newtio.c_cc[VTIME] = 0;  // inter-character timer unused
   newtio.c_cc[VMIN]  = 1;   // blocking read until 1 chars received

   tcflush(tty_fd, TCIFLUSH);
   tcsetattr(tty_fd, TCSANOW, &newtio);


   while (STOP==FALSE)
   {
      res = read(tty_fd,buf,255);  // blocks until char received
      buf[res] = 0;

      if ((buf[0]==0x0d || buf[0]==0x0a))
      {
         if (prevlf==FALSE) { sendAscii(0x0a); prevlf = TRUE; }
         else { prevlf = FALSE; }
      }
      else
      {
         //printf("%s:%d\n", buf, (int)buf[0]);
         //fflush(stdout);
         sendAscii(buf[0]);
         prevlf = FALSE;
      }
   }

   tcsetattr(tty_fd, TCSANOW, &oldtio); // Restore old modem settings

}




int main(argc, argv)
         int argc;
         char **argv;
{
   int c;
   static int showusage = FALSE;
   static int daemonize = TRUE;
   int argerror = FALSE;
   long baudrate = DEFAULT_BAUDRATE;
   char *ttydevice;
   char *uinputdevice = DEFAULT_UINPUT;
   char *pidfile = DEFAULT_PIDFILE;
   pid_t pid;
   int option_index = 0;
   FILE *pid_fd;


   //signal(SIGHUP, signal_handler);
   //signal(SIGTERM, signal_handler);


   // ************
   // GET OPTIONS FROM COMMAND LINE

   static struct option long_options[] =
   {
      {"help",      no_argument,       &showusage, TRUE},
      {"baud",      required_argument, 0,          'b'},
      {"no-daemon", no_argument,       &daemonize, FALSE},
      {"uinput",    required_argument, 0,          'u'},
      {"pid-file",  required_argument, 0,          'p'},
      {0, 0, 0, 0}
   };

   opterr = 0;


   while ((c = getopt_long (argc, argv, "b:u:np:hH", long_options, &option_index)) != -1)
   {
      switch (c)
      {
         case 0:
            // If this option set a flag, do nothing else now
            if (long_options[option_index].flag != 0) break;

            fprintf(stderr, "Unrecognized option: %s", long_options[option_index].name);
            if (optarg) fprintf(stderr, " with arg %s", optarg);
            fprintf(stderr, "\n");
            fprintf(stderr, "Try %s --help\n", argv[0]);
            argerror = TRUE;
            break;

         default:
            fprintf(stderr, "Unrecognized option: %c", optopt);
            if (optarg) fprintf(stderr, " with arg %s", optarg);
            fprintf(stderr, "\n");
            fprintf(stderr, "Try %s --help\n", argv[0]);
            argerror = TRUE;
            break;


         case 'b':
            switch(atoi(optarg))
            {
               case 38400:
                  baudrate = B38400;
                  break;
               case 19200:
                  baudrate = B19200;
                  break;
               case 9600:
                  baudrate = B9600;
                  break;
               case 4800:
                  baudrate = B4800;
                  break;
               case 2400:
                  baudrate = B2400;
                  break;
               case 1200:
                  baudrate = B1200;
                  break;
               case 600:
                  baudrate = B600;
                  break;
               case 300:
                  baudrate = B300;
                  break;

               default:
                  fprintf(stderr, "Unknown baud rate: %s\n", optarg);
                  argerror = TRUE;
            }
            break;

         case 'u':
            uinputdevice = optarg;
            break;

         case 'p':
            pidfile = optarg;
            break;

         case 'n':
            daemonize = FALSE;
            break;

         case 'h':
         case 'H':
            showusage = TRUE;
            break;
      }

   }


   if (showusage == TRUE)
   {
      printf("Usage: %s [OPTION] TTY_DEVICE\n", argv[0]);
      printf("A generic barcode reader keyboard wedge.\n\n");
      printf("TTY_DEVICE is the serial device to listen on.\n\n");
      printf("OPTIONS:\n");
      printf("  -b, --baud=BAUDRATE      : use BAUDRATE as the baud rate [default: 9600]\n");
      printf("  -u, --uinput=UINPUT_DEV  : location of uinput device [default: %s]\n", DEFAULT_UINPUT);
      printf("  -n, --no-daemon          : don't fork off as a daemon\n");
      printf("  -p, --pid-file=PIDFILE   : when daemonized, create PIDFILE pid file [default: %s]\n", DEFAULT_PIDFILE);
   }
   else
   {
      if (optind == argc - 1)
      {
         ttydevice = argv[optind++];
      }
      else
      {
         fprintf(stderr, "Must specify serial device.\nTry %s --help\n", argv[0]);
         argerror = TRUE;
      }
   }

   if (argerror  == TRUE) exit(-1);
   if (showusage == TRUE) exit(0);




   //
   // ******************


   // Open the serial port and uinput device before we fork so
   // we can throw an error if the devices don't exist

   // attempt to open the serial port
   tty_fd = open(ttydevice, O_RDONLY | O_NOCTTY);
   if (tty_fd <0) {perror(ttydevice); exit(-1); }


   if (setup_uinput_device(uinputdevice))
   {
      close(tty_fd);
      exit(-1);
   }


   // Now fork to the background

   if (daemonize)
   {
      pid = fork();

      if (pid < 0)
      {
         exit(EXIT_FAILURE);
      }

      if (pid > 0)
      {
         pid_fd = fopen(pidfile, "w");
         if (pid_fd)
         {
            fprintf(pid_fd, "%d\n", pid);
            fclose(pid_fd);
         }
         else
         {
            perror(pidfile);
            kill(pid, SIGTERM);
            exit(EXIT_FAILURE);
         }

         exit(EXIT_SUCCESS);
      }


      close(STDIN_FILENO);
      close(STDOUT_FILENO);
      close(STDERR_FILENO);
   }





   listen(baudrate);


   destroy_uinput_device();
   close(tty_fd);

   exit(0);
}
