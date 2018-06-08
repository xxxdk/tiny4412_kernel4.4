/* dnw2 linux main file. This depends on libusb.
 * *
 * * Author: Fox <hulifox008@163.com>
 * * License: GPL
 * *
 * */
#include <stdio.h>
#include <usb.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define TINY4412_SECBULK_IDVENDOR   0x04e8
#define TINY4412_SECBULK_IDPRODUCT  0x1234

// #define TINY4412_RAM_BASE       0x60000000

// TINY4412
// #define RAM_BASE            TINY4412_RAM_BASE
#define VENDOR_ID           TINY4412_SECBULK_IDVENDOR
#define PRODUCT_ID          TINY4412_SECBULK_IDPRODUCT

static long ram_base = 0x40000000;

struct usb_dev_handle * open_port()
{
  struct usb_bus *busses, *bus;

  usb_init();
  usb_find_busses();
  usb_find_devices();

  busses = usb_get_busses();
  for(bus=busses;bus;bus=bus->next)
  {
      struct usb_device *dev;
      for(dev=bus->devices;dev;dev=dev->next)
      {
          if( VENDOR_ID==dev->descriptor.idVendor
          &&  PRODUCT_ID==dev->descriptor.idProduct)
          {
              printf("Target usb device found!\n");
              struct usb_dev_handle *hdev = usb_open(dev);
              if(!hdev)
              {
                  perror("Cannot open device");
              }
              else
              {
                  if(0!=usb_claim_interface(hdev, 0))
                  {
                      perror("Cannot claim interface");
                      usb_close(hdev);
                      hdev = NULL;
                  }
              }
              return hdev;
          }
      }
  }

  printf("Target usb device not found!\n");

  return NULL;
}

void usage()
{
    printf("Usage: dwn [-a download_addr] <-f filename>\n\n");
}

unsigned char* prepare_write_buf(char *filename, unsigned int *len)
{
    unsigned char *write_buf = NULL;
    struct stat fs;

    int fd = open(filename, O_RDONLY);
    if(-1==fd)
    {
        perror("Cannot open file");
        return NULL;
    }
    if(-1==fstat(fd, &fs))
    {
        perror("Cannot get file size");
        goto error;
    }
    write_buf = (unsigned char*)malloc(fs.st_size+10);
    if(NULL==write_buf)
    {
        perror("malloc failed");
        goto error;
    }

    if(fs.st_size != read(fd, write_buf+8, fs.st_size))
    {
        perror("Reading file failed");
        goto error;
    }
    unsigned short sum = 0;
    int i;
    for(i=8; i<fs.st_size+8; i++)
    {
        sum += write_buf[i];
    }
    printf("Filename : %s\n", filename);
    printf("Filesize : %d bytes\n", (int)fs.st_size);
    printf ("Sum is %x\n",sum);

    *((u_int32_t*)write_buf) = ram_base;        //download address
    *((u_int32_t*)write_buf+1) = fs.st_size + 10;    //download size;
    write_buf [fs.st_size + 8] = sum & 0xff;
    write_buf [fs.st_size + 9] = sum >> 8;
    *len = fs.st_size + 10;
    return write_buf;

error:
    if(fd!=-1) close(fd);
    if(NULL!=write_buf) free(write_buf);
    fs.st_size = 0;
    return NULL;

}

int main(int argc, char *argv[])
{
    int oc;
    long addr = 0;
    unsigned int len = 0;
    unsigned int remain = 0;
    unsigned int towrite;
    unsigned char* write_buf;

    if(5!=argc)
    {
        usage();
        return 1;
    }

    struct usb_dev_handle *hdev = open_port();
    if(!hdev)
    {
        return 1;
    }

    while ((oc = getopt(argc, argv, "ha:f:")) != -1){
      switch (oc){
        case 'h':
          usage();
          return 0;

        case 'a':
          // addr may be hex or dec
          if (optarg[0]=='0' && (optarg[1]=='x'|| optarg[1]=='X')){
            sscanf(optarg, "%lx", &addr);
            printf("get hex addr: 0x%lx\n", addr);
          }
          else{
            addr = atoll(optarg);
          }

          // check addr
          if (addr <=0 || addr>0xfffffffe){
            printf("Please check your addr(0x%lx), it may be wrong!", addr);
            return -1;
          }
          else
          {
            ram_base = addr;
          }
          break;

        case 'f':
          write_buf = prepare_write_buf(optarg, &len);
          if(NULL==write_buf) return 1;
          remain = len;
          printf("Writing data ...\n");
          while(remain)
          {
              towrite = remain>512 ? 512 : remain;
              printf("towrite %d\n", towrite);
              if(towrite != usb_bulk_write(hdev, 0x02, write_buf+(len-remain), towrite, 3000))
              {
                  printf("\ntowrite %d\n", towrite);
                  perror("usb_bulk_write failed");
                  break;
              }
              remain-=towrite;
              printf("towrite %d\n", towrite);
              printf("remain %d\n", remain);
              printf("\r %d%%\t %d bytes    ", (len-remain)*100/len, len-remain);
              fflush(stdout);
          }
          if(0==remain) printf("Done!\n");
          return 0;

        default:
          printf("Unknow options, please see help \n");
          usage();
          return -1;
      }
    }
}
