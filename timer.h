//states
#define NUM_MACHINES 2
#define RDY   0
#define LOAD  2
#define RUN   4
#define PAUSE 8

//transitions
#define START_TIMER     's'
#define RESET_TIMER     'r'
#define LOAD_TIMER      'l'
#define PAUSE_TIMER     'p'
#define CONTINUE_TIMER  'c'

//names
#define  DEVICE_NAME_1 "timerf"    ///< The device will appear at /dev/timerf using this value
#define  DEVICE_NAME_2 "timerr"    ///< The device will appear at /dev/timerr using this value
#define  CLASS_NAME  "timer"        ///< The device class -- this is a character device driver

#define MESSAGE_LENGTH 255

//State Strings
static const char ready_state[]  = "READY"; 
static const char run_state[]    = "RUNNING"; 
static const char load_state[]   = "LOADED"; 
static const char pause_state[]  = "PAUSED"; 

//prototypes
int register_device(char* name);
int register_device_major_number(void);
int read_timerf(const char *buffer);
int read_timerr(const char *buffer);
void write_timerf(void);
void write_timerr(void);
int jiffies_to_seconds(long jiffies);
void on_ready_timerf(void);
void on_ready_timerr(void);
void on_load(void);
void on_pause(void);
void on_run(void);


// The prototype functions for the character driver -- must come before the struct definition
static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

/** @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
};

struct state_machine{
   char *name;
   dev_t id;
   int state;
   u64 start_jiffies;
   u64 pause_jiffies;
   int loaded_value;
   int (*read) (const char*);
   void (*write) (void);
};

//vars
static int    majorNumber;                  ///< Stores the device number -- determined automatically
static int    minorNumber = 0; 
static struct class*  timerClass  = NULL; ///< The device-driver class struct pointer
static struct device* timerDevice = NULL; ///< The device-driver device struct pointer

char cmd_char;
unsigned long cmd_value;
int send;
struct state_machine *current_device;
u64 jiffies_pause_start = 0;
struct state_machine machines[NUM_MACHINES];

//MACROS
#undef PDEBUG             /* undef it, just in case */
#ifdef DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "TIMER: " fmt, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif
