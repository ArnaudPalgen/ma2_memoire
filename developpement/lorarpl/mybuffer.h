
typedef struct mybuffer
{
    int datasize;
    void* buffer;
    int w_i;// index to write in the buffer
    int r_i;// index to read in the buffer 
    int current_item;// current size of the buffer
    int max_item;
} myqueue;

void queue_init(myqueue *buffer, int datasize, int max_size, void*data);
int queue_append(myqueue *buffer, void* data);
void queue_get(myqueue *buffer, void* dest);
int queue_pop(myqueue *buffer, void* dest);