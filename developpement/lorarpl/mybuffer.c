#include <stdio.h>
#include "contiki.h"
#include "mybuffer.h"


void queue_init(myqueue *buffer, int datasize, int max_item, void*data){
    buffer->datasize = datasize;
    buffer->w_i = 0;
    buffer->r_i=0;
    buffer->current_item = 0;
    buffer->max_item = max_item;
    buffer->buffer=data;
}

int queue_append(myqueue *buffer, void* data){
    if(buffer->current_item >= buffer->max_item){
        return 1;
    }
    void* cell = buffer->buffer+buffer->w_i;
    memcpy(cell,data,buffer->datasize);
    if(buffer->w_i == buffer->max_item-1){
        buffer->w_i = 0;
    }else{
        buffer->w_i=buffer->w_i+buffer->datasize;
    }
    buffer->current_item++;
    return 0;
}

void queue_get(myqueue *buffer, void* dest){

    void* cell = buffer->buffer+buffer->r_i;
    memcpy(dest, cell, buffer->datasize);
}

int queue_pop(myqueue *buffer, void* dest){
    if(buffer->current_item<=0){
        return 1;
    }

    void* cell = buffer->buffer+buffer->r_i;
    memcpy(dest, cell, buffer->datasize);

    if(buffer->r_i == buffer->max_item-1){
        buffer->r_i = 0;
    }else{
        buffer->r_i=buffer->r_i+buffer->datasize;
    }
    buffer->current_item--;
    return 0;
    
}
