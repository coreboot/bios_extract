typedef struct {
    FILE *infile;
    FILE *outfile;
    unsigned long original;
    unsigned long packed;
    int dicbit;
    int method;
} interfacing;

void decode(interfacing interface);
