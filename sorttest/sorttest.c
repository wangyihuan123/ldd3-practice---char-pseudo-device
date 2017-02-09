/*
 * lifotest.c -- simple test of lifo device
 *
 * Ted Baker, FSU, 2010
 * This is a work in progress.
 * 
 * ($Id: lifotest.c,v 1.1 2010/05/22 23:30:13 baker Exp baker $)
 *
 * sorttest.c -- modified lifotest.c to create a test for a sorting device.
 * 
 * Sarah Diesburg, FSU, 2013
 * This is still a work in progress.
 *
 */

#define USE_GOTOS
#include <stdlib.h>
#include <unistd.h>
#include <aio.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "scull.h"
#include "tests.h"

char *fname = NULL;
int fd;

#define N 1000
/* number of characters written in a block */
char outbuf[N+1];
char inbuf [N+1];

#define NN 100
/* number of characters written by each writer, and read by each reader */
char outbufc[NN+1];

#define CONCURRENCY 2
/* number of readers and number of writers */
int  out_histogram[26]; /* implicitly initialized to zero */
int  **in_histogram;

void writer (int self) { /* to be executed as child process */
        int nwritten, nleft, trywrite;
        char *p;
        char self_id [10];

        sprintf (self_id, "w%d", self);
        NEW_TEST (self_id, "writer"); 
        NEW_STEP (a, "open device for writing");
        CHECK (fd = open (fname, O_WRONLY));
        p = outbufc;
        NEW_STEP (b, "writing out data, in random-sized chunks");
        MESSAGE ("to write: %s.", p);
        nleft = NN;
        while (nleft > 0) {
                trywrite = (rand() % nleft) + 1;
                MESSAGE ("trying to write %d chars", trywrite);
                nwritten = write (fd, p, trywrite);
                if (nwritten == 0) break;
                ASSERT (nwritten > 0);
                p += nwritten;
                nleft -= nwritten;
                MESSAGE ("wrote %d chars", nwritten);
                MESSAGE ("to write: %s.", p);
        }
        if (nwritten < 0) {
                FAILURE ("write : %d", nwritten);
                close (fd);
                RECOVER;
        }
        NEW_STEP (c, "close");
        close (fd);
}

void reader (int self) { /* to be executed as a child process */
        char inbufc[NN+1];
        int nread, nleft;
        char *p = inbufc;
        int tryread;
        char self_id [10];
        int c;

        sprintf (self_id, "r%d", self);
        NEW_TEST (self_id, "reader"); 
        NEW_STEP (a, "open device for reading");
        CHECK (fd = open (fname, O_RDONLY));
        NEW_STEP (b, "read in buffer, in random-sized chunks");
        nleft = NN;
        while (nleft > 0) {
                tryread = (rand() % nleft) + 1;
                MESSAGE ("trying to read %d chars", tryread);
                nread = read (fd, p, tryread);
                if (nread == 0) break;
                ASSERT (nread > 0);
                nleft -= nread;
                p += nread;
                MESSAGE("read %d chars", nread);
        }
        if (nread < 0)
                FAILURE("read : %d", nread);
        ASSERT (p == inbufc + NN);
        for (p = inbufc; p < inbufc + NN; p++) {
                c = *p - 'A';
                MESSAGE ("in_histogram at offset %ld", (char *) &(in_histogram[self][c]) - (char *) in_histogram);
                in_histogram[self][c]++;
                MESSAGE ("in_histogram value %d", in_histogram[self][c]);
        }
        NEW_STEP (c, "close");
        close (fd);
}

void run_many () {
        pid_t childpid[2 * CONCURRENCY];

        NEW_STEP (a, "place shared output string into buffer");
        {       char *p;
                char c;
                for (p = outbufc; p < outbufc + NN; p++) {
                        c = (char) (((unsigned long) p) % 26);
                        *p = 'A' + c;
                        out_histogram [(int) c] += CONCURRENCY;
                }
        *p = 0;
        }

        NEW_STEP (b, "set up shared histograms");
        {       const int pagesize = getpagesize();
                int i;
                void *address; 
                const int plist_size = sizeof (int *) * CONCURRENCY;
                const int row_size = sizeof (int) * 26;
                const int len = (((plist_size + CONCURRENCY * row_size) + pagesize) / pagesize) * pagesize;
                MESSAGE ("shared memory region length = %d", len);
                CHECKPTR (address = mmap(0, len, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0));
                ASSERT (address);
                MESSAGE ("address = %p", address);
                ASSERT (*((int *) address) == 0);
                in_histogram = (int **) address;
                for (i = 0; i < CONCURRENCY; i++) {
                        in_histogram[i] = (int *) ((char *) address + plist_size + i * row_size);
                }
                ASSERT (in_histogram[0][0] == 0);
        }

        NEW_STEP (c, "create readers");
        { int i;
                for (i = 0; i < CONCURRENCY; i++) {
                        CHECK (childpid[i] = fork ());
                        if (!childpid[i]) { /* executed only by the child */
                                reader (i);
                                exit (0);
                        }
                }
        }

        NEW_STEP (d, "create writers");
        { int i;
                for (i = CONCURRENCY; i < 2 * CONCURRENCY; i++) {
                        CHECK (childpid[i] = fork ());
                        if (!childpid[i]) { /* executed only by the child */
                                writer (i);
                                exit (0);
                        }
                }
        }

        NEW_STEP (e, "wait for child processes to terminate");
        { int i;
          int child_status;
          char msg_buf[100];
                for (i = 0; i < 2 * CONCURRENCY; i++) {
                        MESSAGE ("checking childpid = %d", childpid[i]);
                        CHECK (waitpid (childpid[i], &child_status, 0));
                        if (WIFEXITED (child_status) && !WEXITSTATUS (child_status)) continue;
                        PUT_STATUS (msg_buf, sizeof (msg_buf), child_status);
                        MESSAGE ("child failed id = %d", i);
                        MESSAGE ("child failed termination reason = %s", msg_buf);
                        kill (childpid[i], SIGKILL);
                }
        }

        NEW_STEP (f, "check histograms");
        { int i, j, t;
                for (i = 0; i < 26; i++) {
                        for (j = 0; j < CONCURRENCY; j++) {
                                t = in_histogram[j][i];
                                out_histogram [i] -= t;
                        }
                }
                for (i = 0; i < 26; i++) {
                        MESSAGE ("i = %c", (char) i);
                        MESSAGE ("out_histogram [i] = %d", out_histogram[i]);
                        ASSERT (out_histogram [i] == 0);
                }
        }
}


int timed_out = 0;
void alarm_handler (int signal) {
        timed_out++; 
}

int main (int argc, char **argv)
{
        GET_VERBOSITY;

        fname = "/dev/scullsort";   
        NEW_DEVICE (fname);
        NEW_TEST (1, "Does read behave in a SORT manner, at the character level?"); 
        NEW_STEP (a, "open device for update");
        CHECK (fd = open (fname, O_RDWR));
        NEW_STEP (b, "write \"hgfedcba\"");
        CHECKIO (write (fd, "hgfedcba", 8), 8);
        NEW_STEP (c, "try to read back 4 characters; expect 4")
        CHECKIO (read (fd, inbuf, 4), 4);
        inbuf[4] = '\0';
        MESSAGE ("%s", inbuf);
        ASSERT (! strncmp (inbuf, "abcd", 4));

        NEW_TEST (2, "Does read  behave in a SORT manner, at the character level?"); 
        NEW_STEP (a, "try to read back 8 characters; expect 4")
        CHECKIO (read (fd, inbuf, 8), 4);
        ASSERT (! strncmp (inbuf, "efgh", 4));

        NEW_TEST (3, "Does read consume the data?");
        EXTRATEST ("Does read from an empty device block if O_NONBLOCK is not set?");
        NEW_STEP (a, "set up handling for SIGALRM");
        {       struct sigaction act;
                CHECK (sigaction (SIGALRM, NULL, &act));
                act.sa_handler = alarm_handler;
                CHECK (sigaction (SIGALRM, &act, NULL));
        }
        NEW_STEP (b, "set up alarm for one-second timeout");
        timed_out = 0; CHECK (alarm (1));
        NEW_STEP (c, "read operation should block for one second");
        CHECKIO (read (fd, inbuf, 1), -1);
        ASSERT (timed_out); CHECK (alarm (0));

        NEW_TEST (4, "Does read from an empty device return immediately if O_NONBLOCK is set?");
        NEW_STEP (a, "set O_NONBLOCK flag");
        {      int i;
               CHECK ((i = fcntl (fd, F_GETFL)));
               CHECK (fcntl (fd, F_SETFL, i | O_NONBLOCK));
        }
        NEW_STEP (b, "set up alarm for one-second timeout");
        timed_out = 0; CHECK (alarm (1));
        NEW_STEP (c, "initiate read operation that should not block");
        CHECKIO (read (fd, inbuf, 1), -1);
        NEW_STEP (d, "cancel alarm");
        ASSERT(! timed_out); CHECK (alarm (0));

        NEW_TEST (5, "Does write to a full device return immediately if O_NONBLOCK is set?");
        NEW_STEP (a, "fill the device, assuming it can be done within one second");
        timed_out = 0; CHECK (alarm (1));
        {       int i = 1;
                while (i > 0) {
                        i = write (fd, "xxxxxxxxxx", 10);
                        ASSERT (! timed_out);
                }
                ASSERT (i == -1);
                ASSERT ((errno == EAGAIN) || (errno = EWOULDBLOCK));
                i = 1;
                while (i > 0) {
                        i = write (fd, "x", 1);
                        ASSERT (! timed_out);
                }
                CHECK (alarm (0));
                ASSERT (i == -1);
        }
        ASSERT ((errno == EAGAIN) || (errno = EWOULDBLOCK));

/** */
        NEW_TEST (6, "Does write to a full device block until the entire string can be written?");
        EXTRATEST ("Does write operation unblock?");
        NEW_STEP (a, "unset O_NONBLOCK flag");
        {int i;
                CHECK ((i = fcntl (fd, F_GETFL)));
                CHECK (fcntl (fd, F_SETFL, i & ~O_NONBLOCK));
        }
        NEW_STEP (b, "set one-second alarm");
        timed_out = 0; CHECK (alarm (1));
        NEW_STEP (c, "write operation that should block for one second");
        CHECKIO (write (fd, "*", 1), -1);
        ASSERT (timed_out); CHECK (alarm (0));
        NEW_STEP (d, "read one character from the device");
        CHECKIO (read (fd, inbuf, 1), 1);
        NEW_STEP (e, "write operation that should block for one second");
        timed_out = 0; CHECK (alarm (1));
        {      int i = write (fd, "**", 2);
               ASSERT (timed_out); CHECK (alarm (0));
               ASSERT ((i == 1) || ((i = -1) && (errno = EINTR)));
        }
        NEW_STEP (f, "read several characters from the device");
        CHECKIO (read (fd, inbuf, 8), 8);
        NEW_STEP (g, "initiate write operation that should not block");
        timed_out = 0; CHECK (alarm (1));
        CHECKIO (write (fd, "**", 2), 2);
        ASSERT (!timed_out); CHECK (alarm (0));

        NEW_TEST (7, "Does SCULL_IOCRESET empty the device?");
        NEW_STEP (a, "do ioctl to reset device");
        CHECK (ioctl (fd, SCULL_IOCRESET));
        NEW_STEP (b, "set O_NONBLOCK flag");
        {       int i;
                CHECK ((i = fcntl (fd, F_GETFL)));
                CHECK (fcntl (fd, F_SETFL, i | O_NONBLOCK));
        }
        NEW_STEP (c, "set up alarm for one-second timeout");
        timed_out = 0; CHECK (alarm (1));
        NEW_STEP (d, "initiate read operation that should not block");
        CHECKIO (read (fd, inbuf, 1), -1);
        ASSERT ((errno == EAGAIN) || (errno = EWOULDBLOCK));
        NEW_STEP (e, "cancel alarm");
        ASSERT(! timed_out); CHECK (alarm (0));

        NEW_TEST (8, "Are invalid ioctl comands rejected?");
        NEW_STEP (a, "do ioctl to reset device");
        {       int i = ioctl (fd, SCULL_IOCQQUANTUM);
                ASSERT ((i =- -1) && (errno = -EINVAL));
        }
        NEW_STEP (b, "close");
        close (fd);

        NEW_TEST (9, "Does the device allow concurrent access by multiple processes?");
        run_many ();

        return 0;
}
        
