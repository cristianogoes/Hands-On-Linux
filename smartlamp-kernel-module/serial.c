#include <linux/module.h>
#include <linux/usb.h>
#include <linux/slab.h>

MODULE_AUTHOR("DevTITANS <devtitans@icomp.ufam.edu.br>");
MODULE_DESCRIPTION("Driver de acesso ao SmartLamp (ESP32 com Chip Serial CP2102");
MODULE_LICENSE("GPL");


#define MAX_RECV_LINE 100 // Tamanho máximo de uma linha de resposta do dispositvo USB

#define S_BUFF_SIZE 4096
static char *s_buf;
static char *value;

static struct usb_device *smartlamp_device;        // Referência para o dispositivo USB
static uint usb_in, usb_out;                       // Endereços das portas de entrada e saida da USB
static char *usb_in_buffer, *usb_out_buffer;       // Buffers de entrada e saída da USB
static int usb_max_size;                           // Tamanho máximo de uma mensagem USB

#define VENDOR_ID   0x10c4 /* Encontre o VendorID  do smartlamp */
#define PRODUCT_ID  0xea60 /* Encontre o ProductID do smartlamp */
static const struct usb_device_id id_table[] = { { USB_DEVICE(VENDOR_ID, PRODUCT_ID) }, {} };

static int  usb_probe(struct usb_interface *ifce, const struct usb_device_id *id); // Executado quando o dispositivo é conectado na USB
static void usb_disconnect(struct usb_interface *ifce);                           // Executado quando o dispositivo USB é desconectado da USB
static int  usb_read_serial(void);                                                   // Executado para ler a saida da porta serial

MODULE_DEVICE_TABLE(usb, id_table);
bool ignore = true;
int LDR_value = 0;

static struct usb_driver smartlamp_driver = {
    .name        = "smartlamp",     // Nome do driver
    .probe       = usb_probe,       // Executado quando o dispositivo é conectado na USB
    .disconnect  = usb_disconnect,  // Executado quando o dispositivo é desconectado na USB
    .id_table    = id_table,        // Tabela com o VendorID e ProductID do dispositivo
};

module_usb_driver(smartlamp_driver);

// Executado quando o dispositivo é conectado na USB
static int usb_probe(struct usb_interface *interface, const struct usb_device_id *id) {
    struct usb_endpoint_descriptor *usb_endpoint_in, *usb_endpoint_out;

    printk(KERN_INFO "SmartLamp: Dispositivo conectado ...\n");

    // Detecta portas e aloca buffers de entrada e saída de dados na USB
    smartlamp_device = interface_to_usbdev(interface);
    ignore =  usb_find_common_endpoints(interface->cur_altsetting, &usb_endpoint_in, &usb_endpoint_out, NULL, NULL);  // AQUI
    usb_max_size = usb_endpoint_maxp(usb_endpoint_in);
    usb_in = usb_endpoint_in->bEndpointAddress;
    usb_out = usb_endpoint_out->bEndpointAddress;
    usb_in_buffer = kmalloc(usb_max_size, GFP_KERNEL);
    usb_out_buffer = kmalloc(usb_max_size, GFP_KERNEL);

    value = kmalloc(20, GFP_KERNEL);
    s_buf = kmalloc(S_BUFF_SIZE, GFP_KERNEL);
	if (!s_buf)
		return -ENOMEM;

    LDR_value = usb_read_serial();

    printk("LDR Value: %d\n", LDR_value);

    return 0;
}

// Executado quando o dispositivo USB é desconectado da USB
static void usb_disconnect(struct usb_interface *interface) {
    printk(KERN_INFO "SmartLamp: Dispositivo desconectado.\n");
    kfree(usb_in_buffer);                   // Desaloca buffers
    kfree(usb_out_buffer);
    kfree(s_buf);
    kfree(value);
}

static int usb_read_serial() {
    int ret, actual_size;
    int retries = 10;                       // Tenta algumas vezes receber uma resposta da USB. Depois desiste.
    
    int counter = 0;
    
    int i;
    int j=0;
    long val = -1;
    
    // Espera pela resposta correta do dispositivo (desiste depois de várias tentativas)
    while (retries > 0) {
        // Lê os dados da porta serial e armazena em usb_in_buffer
        // usb_in_buffer - contem a resposta em string do dispositivo
        // actual_size - contem o tamanho da resposta em bytes
        
        ret = usb_bulk_msg(smartlamp_device, usb_rcvbulkpipe(smartlamp_device, usb_in), usb_in_buffer, min(usb_max_size, MAX_RECV_LINE), &actual_size, 1000); 

        if (ret) {
            printk(KERN_ERR "SmartLamp: Erro ao ler dados da USB (tentativa %d). Codigo: %d\n", retries--, ret);
            continue;
        }
        
        usb_in_buffer[actual_size] = '\0'; // Assegura que o buffer está terminado em null
        printk(KERN_INFO "Palavra1[%d]: %s", actual_size, usb_in_buffer);

        strcat(s_buf, usb_in_buffer);

        printk(KERN_INFO "S_BUF: %s", s_buf);
        
        if(strstr(usb_in_buffer, "\n") != NULL){
            if (strstr(s_buf, "RES GET_LDR ") != NULL) {
                printk(KERN_INFO ">>>Resposta[%d]: %s", strlen(s_buf), s_buf);

                s_buf[strlen(s_buf)] = '\0';

                for(i = 0; i < strlen(s_buf); i++){

                    if(s_buf[i] == ' '){
                        counter++;
                    }
                    
                    if(counter == 2){
                        i++;
                        break;
                    }
                }
                    
                for(; i < strlen(s_buf); i++) {
                    value[j] = s_buf[i];
                    j++;
                }

                value[j] = '\0';
                value[j-1] = '\0';
                value[j-2] = '\0';
                printk(KERN_INFO "Value[%d]: %s", strlen(value), value);

                kstrtol(value, 10, &val);
                printk(KERN_INFO "Valor do LDR convertido:%d", val);
                
                strcpy(s_buf,"\0");
                strcpy(value,"\0");
                counter = 0;
                j = 0;

                return val;
            }
            strcpy(s_buf,"\0");
        }
    }

    return -1;    
}
