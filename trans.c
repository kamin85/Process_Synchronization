/*
    Scott Kamin
    COP4610 - Operating Systems
    Program Project 3
    10/25/2016

    Program to open specified input file and transfer 4kb blocks to a shared
    memory space using pipes and then transfering the block in shared memory
    to the specified output file. Specified input and output files should have
    identical checksums after termination of program.
*/

#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h> 
#include <sys/wait.h>
#include <sys/types.h>

int main(int argc, char *argv[])
{
    /*      Varibles used in main                          */
    pid_t pid;
    const int SIZE = 4096;      // 4KB 

    // Child write -> fds[1] | Parent read <- fds[0]
    int fds[2];

    // Parent write -> fdk[1] | Child read <- fdk[0]
    int fdk[2];

    // Array for [Block #][Block Size]
    // mesC is for message to CHILD mesP is for message to PARENT
    int mesC[2] = {-1, -1};
    int mesP[2] = {-1, -1};

    // Return value for pipes and error check
    int ret;
    int ret2;

    // Return value for smh_open and error check
    int fd;

    // Address of the shared memory created
    void *addr;

    // Used for the input and output files
    FILE *infp;
    FILE *outfp;
    long fileSize;
    long fullFileSize;
    int fullB;
    int rem;


    char buf[4096];
    char buf2[4096];
    int YorN;
    /*************************************************************************/

    /*      Simple error checking before starting programs operations       */
    if(argc != 3){
        fprintf(stderr, "Incorrect number of arguments.\n");
        shm_unlink("/swk12_cop4610");
        exit(EXIT_FAILURE);
    }
    // check input file exists
    // Open the input and output files
    infp = fopen(argv[1], "r");
    if(infp == NULL){
        perror("Input file was unable to open.\n");
        shm_unlink("/swk12_cop4610");
        exit(EXIT_FAILURE);
    }
    // Check if output file exists and if so ask to overwrite
    if(access(argv[2], F_OK) != -1){
        while((YorN != 78) & (YorN != 110) & (YorN != 89) & (YorN != 121)){
            printf("Output file exists, overwrite? (Y)es or (N)o:\n");
            YorN = getchar();
        }
        if((YorN == 78) | (YorN == 110)){
            perror("Create new file or choose to overwrite existing.\n");
            exit(EXIT_FAILURE);
        }
    }
    // open file if havent exit 
    outfp = fopen(argv[2], "w");

    // ask to overwrite output
    /*************************************************************************/

    /*      Create a pipe and check that there was not an error              */
    ret = pipe(fds);
    ret2 = pipe(fdk);

    if(ret == -1){
        perror("PIPE FAILED\n");
        shm_unlink("/swk12_cop4610");
        exit(EXIT_FAILURE);
    }
    if(ret2 == -1){
        perror("PIPE 2 FAILED\n");
        shm_unlink("/swk12_cop4610");
        exit(EXIT_FAILURE);
    }
    /*************************************************************************/

    
    /*      Fork and make sure there was not an error                        */
    pid = fork();

    if(pid == -1){
        perror("Fork failed.\n");
        shm_unlink("/swk12_cop4610");
        exit(EXIT_FAILURE);
    }

    // set fd to file descriptor of  shared memory
    fd = shm_open("/swk12_cop4610", O_RDWR | O_CREAT, 0777);
    if(fd == -1){
        perror("Shared memory opening error.\n");
        exit(EXIT_FAILURE);
    }
    ftruncate(fd, SIZE);
    addr = mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(addr == MAP_FAILED){
        perror("Mapping failed.\n");
        exit(EXIT_FAILURE);
    }


    //////////////////////////////////////////////////

    /* Store the size of the entire file                                     */
    fseek(infp, 0, SEEK_END);
    fileSize = ftell(infp);
    fullFileSize = fileSize;
    printf("Size (in kb) of file to transfer: %ld\n", fullFileSize);
    // set back to beggining
    fseek(infp, 0, SEEK_SET);
    /*************************************************************************/

    // Store the number of whole 4kb blocks to transfer
    fullB = fileSize / SIZE;
    printf("Number of blocks to transfer: %i\n", fullB+1);

    /*************************************************************************/

    /*      Child process reads from shared memory and writes to output file */
    if(pid == 0){
        close(fds[0]);
        close(fdk[1]);
        int nothing = 0;
        ssize_t reed = 0;
        if(fullB != 0){
            for(int blk = 1; blk <= fullB; ++blk){
                reed = 0;
                while(reed == 0){
                    nothing = nothing + 1;
                    reed = read(fdk[0], mesC, 16);
                }
                memcpy(buf2, addr, SIZE);
                fwrite(buf2, 1, SIZE, outfp);
                mesP[0] = blk;
                mesP[1] = SIZE;
                write(fds[1], mesP, 16);
            }   
            reed = 0;
            nothing = 0;
            while(reed == 0){
                nothing = nothing + 1;
                reed = read(fdk[0], mesC, 16);
            }
        }
        rem = fullFileSize - (SIZE * fullB);
        memcpy(buf2, addr, rem);
        fwrite(buf2, 1, rem, outfp);
        mesP[0] = fullB + 1;
        mesP[1] = SIZE;
        write(fds[1], mesP, 16);

        nothing = 0;
        reed = 0;
        while(reed == 0){
            nothing = nothing + 1;
            reed = read(fdk[0], mesC, 16);
        }
        if(mesC[0] == 0){
            mesP[0] = 0;
            mesP[1] = 0;
            write(fds[1], mesP, 16);
        }
    }
    /*************************************************************************/

    /*      Parent process reads from input file and writes to shared memory */
    else{
        close(fds[1]);
        close(fdk[0]);
        if(fullB != 0){
            for(int blck = 1; blck <= fullB; ++blck){
                if(blck == 1){
                    fread(buf, 1, 4096, infp);
                    fileSize = ftell(infp);
                    memcpy(addr, buf, fileSize);
                    mesC[0] = blck;
                    mesC[1] = SIZE;
                    write(fdk[1], mesC, 16);
                }
                else{
                    fseek(infp, 0, SEEK_CUR);
                    fread(buf, 1, 4096, infp);
                    memcpy(addr, buf, fileSize);

                    mesC[0] = blck;
                    mesC[1] = SIZE;
                    write(fdk[1], mesC, 16);
                }

                int nada = 0;
                ssize_t red = 0;
                while(red == 0){
                    nada = nada + 1;
                    red = read(fds[0], mesP, 16);
                }
            }
        }
        rem = fullFileSize - (SIZE * fullB);
        fseek(infp, 0, SEEK_CUR);
        fread(buf, 1, rem, infp);
        memcpy(addr, buf, rem);
        mesC[0] = fullB+1;
        mesC[1] = rem;
        write(fdk[1], mesC, 16);

        int nada = 0;
        ssize_t red = 0;
        while(red == 0){
            nada = nada + 1;
            red = read(fds[0], mesP, 16);
        }
        mesC[0] = 0;
        mesC[1] = 0;
        write(fdk[1], mesC, 16);

        nada = 0;
        red = 0;
        while(red == 0){
            nada = nada + 1;
            red = read(fds[0], mesP, 16);
        }
        if(mesP[0] == 0){
            printf("Child returned 0 - DONE\n");
        }

        /*********************************************************************/
    }
    /*************************************************************************/

    wait(NULL);
    close(fdk[0]);
    close(fdk[1]);
    close(fds[0]);
    close(fds[1]);
    shm_unlink("/swk12_cop4610");
    // CLOSE ALL INPUT AND OUTPUT FILES
    fclose(infp);
    fclose(outfp);
    /* END OF PROGRAM */
    return 0;
}