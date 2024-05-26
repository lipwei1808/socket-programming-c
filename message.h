#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NAME_LENGTH 32
#define MAX_DATA_LENGTH 1024

struct message
{
  int name_length;
  int data_length;
  char name[MAX_NAME_LENGTH];
  char data[MAX_DATA_LENGTH];
} typedef Message;

Message *message_create(int name_length, int data_length, char *name, char *data);

Message *message_unmarshal(char *data);

void message_marshal(Message* msg, char* buffer);

void message_delete(Message* msg);

#endif