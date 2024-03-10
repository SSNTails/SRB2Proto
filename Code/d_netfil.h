#define TXPACKETSIZE 256

typedef enum {
    file           = 0,
    z_free_methode    ,
    free_methode      ,
    nofree
} freemethode_t;


void SendFile(int node,char *filename);
void SendRam(int node,byte *data, ULONG size,freemethode_t freemethode);
void FiletxTicker(void);
void Got_Filetxpak(void);
boolean fileexist(char *filename,time_t time);

// search a file in subtree, return true if found
int recsearch(char *filename,time_t timestamp);
void nameonly(char *s);