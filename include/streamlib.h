TKMsgHeader *streamCreateMsg(TKStreamPointer *pointer, TKMessageID identifier, unsigned int length);
TKMsgHeader *streamReadMsg(TKStreamPointer *pointer);
void streamMarkMessageRead(TKMsgHeader *msg);

