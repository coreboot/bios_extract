typedef struct {
    FILE *infile;
    FILE *outfile;
    unsigned long original;
    unsigned long packed;
} interfacing;

void decode(interfacing interface);
