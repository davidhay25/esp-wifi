#ifndef _FETCH_H_
#define _FETCH_H_

struct GetParms
{
    void (*OnGotData)(char *incomingBuffer, char* output);
    char message[300];
} ;

void myWifiInit();
void myGET(char* url, struct GetParms *getParms);
void myPOST(char *url, char *data);




#endif