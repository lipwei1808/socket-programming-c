#include "message.h"

Message* message_create(int name_length, int data_length, char* name, char* data) {
  Message* msg = malloc(sizeof(Message));
  msg->name_length = name_length;
  msg->data_length = data_length;
  strncpy(msg->name, name, MAX_NAME_LENGTH);
  strncpy(msg->data, data, MAX_DATA_LENGTH);
  return msg;
}

Message* message_unmarshal(char* buffer) {
  int name_length = *buffer;
  int data_length = *(buffer + 4);
  char name[name_length];
  char data[data_length];
  strncpy(name, buffer + 8, name_length);
  strncpy(data, buffer + 8 + name_length, data_length);
  name[strlen(name)] = '\0';
  data[strlen(data)] = '\0';
  return message_create(name_length, data_length, name, data);
}

void message_marshal(Message* msg, char* buffer) {
  memcpy(buffer, &msg->name_length, 4);
  memcpy(buffer + 4, &msg->data_length, 4);
  memcpy(buffer + 8, msg->name, msg->name_length);
  memcpy(buffer + 8 + msg->name_length, msg->data, msg->data_length);
}

void message_delete(Message* msg) {
  free(msg);
}

