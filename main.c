#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MIN_BF_MEM_SIZE 30000

struct bf_op
{
    int shift;
    int offset;
    int *d;
    int size;
    struct bf_op *op;
    int ch;
    int op_ind;
    int *db;
    int dbsize;
    int linear;
};

static struct option longopts[] =
{
    { "file", required_argument, NULL, 'f' },
    { "help", no_argument, NULL, 'h' },
    { NULL, 0, NULL, 0 }
};

/**
 * @brief Simple zalloc function. Caveat: This function does NOT zero all memory
 * it reallocates ptr and 0's out the new memory instead.
 */
static void *zalloc(void *ptr, int new_size, int orig_size)
{
    ptr = realloc(ptr, new_size);
    if (!ptr)
    {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    /* Only zero out the new memory */
    memset((char *)ptr + orig_size, 0, new_size - orig_size);

    return ptr;
}

#define zalloc_int(ptr, new_size, orig_size)    \
    zalloc(ptr, (new_size) * sizeof(int), (orig_size) * sizeof(int))

static int get_bf_char(FILE *infile)
{
    int c = getc(infile);

    for (;; c = getc(infile))
    {
        if (c == EOF)
            return c;
        /* Ignore non BF characters */
        if (!strchr("<>+-.,[]", c))
            continue;
        return c;
    }
}

static int bf_consume(FILE *infile, struct bf_op *op)
{
    int i;
    int mem_ind = 0;
    int c = op->ch;

    if (strchr("[]", c))
        c = get_bf_char(infile);

    op->size = 1;
    op->d = zalloc_int(NULL, 1, 0);
    op->offset = 0;
    op->dbsize = 0;
    op->db = NULL;

    for (;; c = get_bf_char(infile))
    {
        if (c == EOF)
            break;
        /* Can't coalesce anymore operations without breaking something */
        if (strchr(",.[]", c))
            break;

        op->db = zalloc_int(op->db, op->dbsize + 1, op->dbsize);
        op->db[op->dbsize++] = c;

        switch (c)
        {
            case '+':
                op->d[mem_ind]++;
                break;
            case '-':
                op->d[mem_ind]--;
                break;
            case '<':
                if (mem_ind > 0)
                    mem_ind--;
                else /* Left extend the tape */
                {
                    op->offset--;
                    op->d = zalloc_int(op->d, op->size + 1, op->size);
                    /* Shift data contents because of extension */
                    for (i = op->size; i > 0; i--)
                        op->d[i] = op->d[i - 1];
                    op->d[0] = 0;
                    op->size++;
                }
                break;
            case '>':
                mem_ind++;
                if (mem_ind >= op->size)
                {
                    op->d = zalloc_int(op->d, op->size + 1, op->size);
                    op->size++;
                }
                break;
            default:
                fprintf(stderr, "UNEXPECTED CHARACTER %c\n", c);
                exit(1);
        }
    }
    op->shift = mem_ind + op->offset;

    while (op->size && !op->d[op->size - 1])
        op->size--;
    while (op->size && !op->d[0])
    {
        op->size--;
        for (i = 0; i < op->size; i++)
            op->d[i] = op->d[i + 1];
        op->offset++;
    }

    return c;
}

static int bf_int(FILE *infile)
{
    struct bf_op *op_arr = NULL;
    struct bf_op *op;
    struct bf_op *op_end;
    int bracket_count;
    int i;
    int *mem;
    int new_mem_size;
    int delete;
    int size;
    int mem_ind = 0;
    int mem_size = MIN_BF_MEM_SIZE;
    int c = get_bf_char(infile);

    for (size = 0; ; size++)
    {
        if (c == EOF)
            break;

        op_arr = zalloc(op_arr, (size + 1) * sizeof(struct bf_op),
                        size * sizeof(struct bf_op));
        op_arr[size].ch = c;

        /* Input or output, no more work to be done */
        if (strchr(",.", c))
        {
            c = get_bf_char(infile);
            continue;
        }

        /* End of loop marker */
        if (c == ']')
        {
            bracket_count = 1;
            i = size;
            while (i >= 0 && bracket_count)
                if (i--)
                    bracket_count += ((op_arr[i].ch == ']') -
                                      (op_arr[i].ch == '['));
            if (i < 0)
            {
                fprintf(stderr, "Unbalanced loop brackets\n");
                exit(1);
            }

            op_arr[i].op_ind = size;
            op_arr[size].op_ind = i;
        }
        c = bf_consume(infile, op_arr + size);
    }

    /* Linear optimization of op's */
    for (i = 0; i < size; i++)
    {
        op_arr[i].op = &op_arr[op_arr[i].op_ind];
        if (op_arr[i].ch == '[' && op_arr[i].op_ind == i + 1 && !op_arr[i].shift
            && op_arr[i].offset <= 0)
        {
            op_arr[i].linear = -op_arr[i].d[-op_arr[i].offset];
            if (op_arr[i].linear < 0)
            {
                fprintf(stderr, "WARNING: Program contains infinite loop\n");
                op_arr[i].linear = 0;
            }
        }
        else
            op_arr[i].linear = 0;
    }

    mem = zalloc_int(NULL, mem_size, 0);
    op = op_arr;
    op_end = op_arr + size;
    if (op_end < op)
    {
        fprintf(stderr, "Overflow detected in op_arr!!!\n");
        exit(1);
    }

    for (; op != op_end; op++)
    {
        if (op->ch == ']')
        {
            if (mem[mem_ind])
                op = op->op;
        }
        else if (op->ch == '[')
        {
            if (!mem[mem_ind])
                op = op->op;
        }
        else if (op->ch == ',')
        {
            mem[mem_ind] = getchar();
            continue;
        }
        else if (op->ch == '.')
        {
            putchar(mem[mem_ind]);
            continue;
        }

        if (op->size)
        {
            new_mem_size = mem_ind + op->size + op->offset;
            if (new_mem_size > mem_size)
            {
                mem = zalloc_int(mem, new_mem_size, mem_size);
                mem_size = new_mem_size;
            }

            if (op->linear)
            {
                delete = mem[mem_ind] / op->linear;
                for (i = 0; i < op->size; i++)
                    mem[mem_ind + op->offset + i] += delete * op->d[i];
            }
            else
                for (i = 0; i < op->size; i++)
                {
                    mem[mem_ind + op->offset + i] += op->d[i];
                }
        }

        if (op->shift > 0)
        {
            new_mem_size = mem_ind + op->shift + 1;
            if (new_mem_size > mem_size)
            {
                mem = zalloc_int(mem, new_mem_size, mem_size);
                mem_size = new_mem_size;
            }
        }
        mem_ind += op->shift;
    }

    for (i = 0; i < size; i++)
    {
        free(op_arr[i].d);
        free(op_arr[i].db);
    }
    free(mem);
    free(op_arr);

    return 0;
}

static void usage()
{
    printf("Usage: bfint [-h] [-f input_file]\n");
    printf("-h: Print this usage message and exit\n");
    printf("-f input_file: take input from file rather than stdin\n");
}

int main(int argc, char **argv)
{
    int c;
    FILE *infile = stdin;
    int ret;

    while ((c = getopt_long(argc, argv, "hf:", longopts, NULL)) != -1)
    {
        switch (c)
        {
            case 'h':
                usage();
                return 0;
            case 'f':
                infile = fopen(optarg, "r");
                if (!infile)
                {
                    perror("fopen");
                    return 1;
                }
                break;
            default:
                fprintf(stderr, "Unsupported flag %c\n", c);
                usage();
                return 1;
        }
    }

    ret = bf_int(infile);

    fclose(infile);

    return ret;
}
