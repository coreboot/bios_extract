typedef struct {
    FILE *infile;
    FILE *outfile;
    uint32_t original;
    uint32_t packed;
    int dicbit;
    int method;
} interfacing;

void decode(interfacing interface);
