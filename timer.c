/**
 * @file   timer.c
 * @author Florian Heuer
 * @date   11 December 2016
 * @version 0.1
 * @brief Chardevice wich counts jiffies forwards and backwards and return it to user, when
 * read from it.
 */

#include <linux/init.h>             // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h>           // Core header for loading LKMs into the kernel
#include <linux/device.h>           // Header to support the kernel Driver Model
#include <linux/kernel.h>           // Contains types, macros, functions for the kernel
#include <linux/fs.h>               // Header for the Linux file system support
#include <asm/uaccess.h>            // Required for the copy to user function
#include <linux/jiffies.h>          // Timerticks lib
#include <linux/slab.h>             // kmalloc

#include "timer.h"          

MODULE_LICENSE("GPL");            ///< The license type -- this affects available functionality
MODULE_AUTHOR("Florian Heuer");    ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("Timer Device");  ///< The description -- see modinfo
MODULE_VERSION("0.1");            ///< A version number to inform users

int 
register_device_major_number(){

   // Try to dynamically allocate a major number for the device -- more difficult but worth it
   majorNumber = register_chrdev(0, CLASS_NAME, &fops);
   if (majorNumber<0){
      PDEBUG("%s failed to register a major number\n", CLASS_NAME);
      return majorNumber;
   }
   PDEBUG("registered correctly with major number %d\n", majorNumber);

   return 0;
}

int 
register_device(char* name){

   dev_t device_identifier = MKDEV(majorNumber, minorNumber);
   
   // Register the device driver
   timerDevice = device_create(timerClass, NULL, device_identifier, NULL, name);
   if (IS_ERR(timerDevice)){               // Clean up if there is an error
      class_destroy(timerClass);           // Repeated code but the alternative is goto statements
      unregister_chrdev(majorNumber, name);
      PDEBUG("Failed to create the device\n");
      return PTR_ERR(timerDevice);
   }
   PDEBUG("TIMER: device(%s) created correctly\n", name);

   //set device attributes
   machines[minorNumber].name  = name;
   machines[minorNumber].id    = device_identifier;
   machines[minorNumber].state = RDY;

   machines[minorNumber].read  = (minorNumber==0)?read_timerf:read_timerr;
   machines[minorNumber].write = (minorNumber==0)?write_timerf:write_timerr;
   
   minorNumber++;

   return 0;
}


/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point.
 *  @return returns 0 if successful
 */
static int __init timer_init(void){

   PDEBUG("Initializing %s on LKM\n", CLASS_NAME);

   // Register the device class
   timerClass = class_create(THIS_MODULE, CLASS_NAME);
   if (IS_ERR(timerClass)){                // Check for error and clean up if there is
      PDEBUG("Failed to register device class\n");

      return PTR_ERR(timerClass);          // Correct way to return an error on a pointer
   }
   PDEBUG("TIMER: device class registered correctly\n");

   register_device_major_number();

   register_device(DEVICE_NAME_1);
   register_device(DEVICE_NAME_2);
   
   return 0;
}

/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit timer_exit(void){
   device_destroy(timerClass, MKDEV(majorNumber, minorNumber-2));     // remove the device
   device_destroy(timerClass, MKDEV(majorNumber, minorNumber-1));     // remove the device
   class_unregister(timerClass);                          // unregister the device class
   class_destroy(timerClass);                             // remove the device class
   unregister_chrdev(majorNumber, CLASS_NAME);             // unregister the major number
   PDEBUG("TIMER: Goodbye from the LKM!\n");
}

/** @brief The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep){
   send=0;

   //map current device
   int i;
   for(i=0; i<NUM_MACHINES; i++){
      if(inodep->i_rdev==machines[i].id){
         current_device = &machines[i];
      }
   }

   PDEBUG("TIMER: Device has been (%s) opened\n", current_device->name);
   return 0;
}

/** @brief This function is called whenever device is being read from user space i.e. data is
 *  being sent from the device to the user. In this case is uses the copy_to_user() function to
 *  send the buffer string to the user and captures any errors.
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the data
 *  @param len The length of the b
 *  @param offset The offset if required
 */
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
   if(send==1) 
         return 0;

   PDEBUG("Read from Device");
   int length = current_device->read(buffer);


    send=1;
    return length;
}

/** @brief This function is called whenever the device is being written to from user space i.e.
 *  data is sent to the device from the user. The data is copied to the message[] array in this
 *  LKM using the sprintf() function along with the length of the string.
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
 */
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){

   //clean input
   char * copy = kmalloc(strlen(buffer) + 1, GFP_KERNEL); 
   strcpy(copy, buffer);
   copy[strcspn(copy, "\n")] = 0;

   cmd_char = copy[0];

   int success = sscanf(copy+1, "%lu", &cmd_value);


   current_device->write();

   PDEBUG("Received (%s) from user! (%c:%d)\n", copy, cmd_char, cmd_value);

   memset(buffer, 0x00, sizeof(buffer));   /* clear buffer */;
   return len;
}

/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep){
   PDEBUG("Device successfully closed\n");
   return 0;
}

void
write_timerf(){
   switch(current_device->state){
      case RDY: 
         on_ready_timerf();
      break;
      case RUN:
         on_run();
      break;  
      case PAUSE:
         on_pause();
      break;
   }
}

void
write_timerr(){
   switch(current_device->state){
      case RDY: 
         on_ready_timerr();
      break;
      case LOAD: 
         on_load();
      break;
      case RUN:
         on_run();
      break;  
      case PAUSE:
         on_pause();
      break;
   } 
}

void 
on_ready_timerf(){
   switch(cmd_char){
      case START_TIMER:
         current_device->start_jiffies = get_jiffies_64();
         current_device->state = RUN;
         //printk(KERN_INFO "TIMER: start timer\n");
         PDEBUG("start timer");
      break;
      default:
         PDEBUG("Eingabe (%c) nicht erlaubt", cmd_char);
      break;
   }
};

void 
on_ready_timerr(){
   switch(cmd_char){
      case LOAD_TIMER:
         current_device->loaded_value = cmd_value;
         current_device->state = LOAD;
         PDEBUG(KERN_INFO "load timer\n");
      break;
      default:
         PDEBUG("Eingabe (%c) nicht erlaubt", cmd_char);
      break;
   }
};


void 
on_pause(void){
   switch(cmd_char){
      case CONTINUE_TIMER:
         current_device->pause_jiffies += get_jiffies_64() - jiffies_pause_start;
         jiffies_pause_start=0;
         current_device->state = RUN;
         PDEBUG(KERN_INFO "continue timer\n");
      break;
      case RESET_TIMER:
         current_device->start_jiffies = 0;
         current_device->state = RDY;
         current_device->pause_jiffies = 0;
         jiffies_pause_start = 0;
         PDEBUG(KERN_INFO "reset timer\n");
      break;
      default:
         PDEBUG("Eingabe (%c) nicht erlaubt", cmd_char);
      break;
   }
};

void 
on_run(void){
   switch(cmd_char){
      case PAUSE_TIMER:
         jiffies_pause_start = get_jiffies_64();
         current_device->state = PAUSE;
         PDEBUG(KERN_INFO "pause timer\n");
      break;
      case RESET_TIMER:
         current_device->start_jiffies = 0;
         current_device->state = RDY;
         current_device->pause_jiffies = 0;
         jiffies_pause_start = 0;
         PDEBUG(KERN_INFO "reset timer\n");
      break;
      default:
         PDEBUG("Eingabe (%c) nicht erlaubt", cmd_char);
      break;
   }
};

void 
on_load(void){
   switch(cmd_char){
      case START_TIMER:
         current_device->start_jiffies = get_jiffies_64();
         current_device->state = RUN;
         PDEBUG(KERN_INFO "start timer\n");
      break;
      case RESET_TIMER:
         current_device->start_jiffies = 0;
         current_device->state = RDY;
         current_device->pause_jiffies = 0;
         jiffies_pause_start = 0;
         PDEBUG(KERN_INFO "reset timer\n");
      break;
      default:
         PDEBUG("Eingabe (%c) nicht erlaubt", cmd_char);
      break;
   }
};

char* get_state(int state){
   switch(state){
      case RDY:
         return ready_state;
      break;
      case LOAD:
         return load_state;
      break;
      case RUN:
         return run_state;
      break;
      case PAUSE:
         return pause_state;
      break;
   }
   return "null";
}

int
read_timerf(const char *buffer){
   char message[255];
   int counter = 0;
   int pause = current_device->pause_jiffies;

   if(current_device->start_jiffies>0){
      counter = get_jiffies_64() - current_device->start_jiffies;
   }

   if(jiffies_pause_start>0){
      pause = current_device->pause_jiffies + get_jiffies_64() - jiffies_pause_start;
   }

   sprintf(message, "Zustand: \t\t%s\nCounter in s: \t\t%d\ndavon Pausen in s: \t%d\nGesamt in s: \t\t%d\n", 
      get_state(current_device->state), jiffies_to_seconds(counter), jiffies_to_seconds(pause), jiffies_to_seconds(counter-pause) ); 
   
   int error_count = copy_to_user(buffer, message, strlen(message));
   return strlen(message);
}

int
read_timerr(const char *buffer){
   char message[255];
   int amount;
   int counter = 0;
   int pause = current_device->pause_jiffies;

   if(current_device->start_jiffies>0){
      counter = (cmd_value*HZ) - (get_jiffies_64() - current_device->start_jiffies);
   }

   if(jiffies_pause_start>0){
      pause = current_device->pause_jiffies + get_jiffies_64() - jiffies_pause_start;
   }

   amount = jiffies_to_seconds(counter+pause);
   sprintf(message, "Zustand: \t\t%s\nCounter in s: \t\t%d\ndavon Pausen in s: \t%d\nGesamt in s: \t\t%d\n", 
      get_state(current_device->state), jiffies_to_seconds(counter), jiffies_to_seconds(pause), (amount<0)?0:amount ); 
   
   int error_count = copy_to_user(buffer, message, strlen(message));
   return strlen(message);
}


int 
jiffies_to_seconds(long jiffies){
   return jiffies/HZ;
}



/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
module_init(timer_init);
module_exit(timer_exit);