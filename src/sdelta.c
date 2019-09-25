/*

sdelta.c was written and copyrighted by Kyle Sallee
You may use it in accordance with the 
Sorcerer Public License version 1.1
Please read LICENSE

sdelta can identify and combine the difference between two files.
The difference, also called a delta, can be saved to a file.
Then, the second of two files can be generated 
from both the delta and first source file.

sdelta is a line blocking dictionary compressor.

*/


#define _GNU_SOURCE

/* for stdin stdout and stderr */
#include <stdio.h>
#include <errno.h>
/* for memcmp */
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef USE_LIBMD
#include <sha.h>
#else
#include <openssl/sha.h>
#endif

#include "input.h"
#include "sdelta.h"

char	magic[]    =  { 0x13, 0x04, 00, 00 };

int	verbosity  =  0;


#define  leap(frog) \
  if ( start >= frog ) { \
    from.block   =   from.index.ordered[where + frog]; \
    fcrc         =  *(QWORD *)( from.index.crc + from.block ); \
    if ( crc.qword  >  fcrc.qword  ) { \
      where  +=  frog; \
      start  -=  frog; \
    } else  start   =  frog - 1; \
  }


void	output_sdelta(FOUND found, TO to, FROM from) {

  DWORD			size, origin, stretch, unmatched_size;
  unsigned char		byte_val;
  int			block;
  DWORD			*dwp;
  unsigned int		offset_unmatched_size;
  SHA_CTX		ctx;

/*
  found.buffer   =  malloc ( to.size );
*/
  found.buffer   =  (unsigned char *) temp. current;

  memcpy( found.buffer,      magic,     4  );
  memcpy( found.buffer + DIGEST_SIZE + 4, from.digest, DIGEST_SIZE );
  found.offset   =  4 + 2 * DIGEST_SIZE;

  dwp = (DWORD *)&to.size;
  found.buffer[found.offset++] = dwp->byte.b3;
  found.buffer[found.offset++] = dwp->byte.b2;
  found.buffer[found.offset++] = dwp->byte.b1;
  found.buffer[found.offset++] = dwp->byte.b0;

  stretch.dword  =  0;
  to.block       =  0;

  for ( block = 0;  block < found.count ; block++ ) {

    stretch.dword  =  found.pair[block].to.dword - to.block;
    to.block       =  found.pair[block].to.dword + found.pair[block].count.dword;

    /*
    printf("to.index.natural[found.pair[block].to].offset  %i\n",
            to.index.natural[found.pair[block].to].offset );
    printf("block                     %i\n", block);
    printf("found.pair[block].count   %i\n", found.pair[block].count);
    printf("found.pair[block].from    %i\n", found.pair[block].from);
    printf("found.pair[block].to      %i\n", found.pair[block].to);
    printf("stretch                   %i\n", stretch.dword);
    printf("\n");
    */

    /*
    printf("\n");
    printf("found.pair[block].to + size %i\n", found.pair[block].to + size);
    printf("to.index.natural[found.pair[block].to + size].offset %i\n",
            to.index.natural[found.pair[block].to + size].offset);
    printf("\n");
    */

    origin.dword  =  found.pair[block].from.dword;
      size.dword  =  found.pair[block].count.dword;

                                         byte_val  = 0x00;
    if ( origin.dword     >= 0x1000000 ) byte_val |= 0xc0;  else
    if ( origin.dword     >= 0x10000   ) byte_val |= 0x80;  else
    if ( origin.dword     >= 0x100     ) byte_val |= 0x40;

    if ( size.dword       >= 0x1000000 ) byte_val |= 0x30;  else
    if ( size.dword       >= 0x10000   ) byte_val |= 0x20;  else
    if ( size.dword       >= 0x100     ) byte_val |= 0x10;

    if   ( stretch.dword  > 0 )  {       byte_val |= 0x02;
      if ( stretch.dword  >= 0x1000000 ) byte_val |= 0x0c;  else
      if ( stretch.dword  >= 0x10000   ) byte_val |= 0x08;  else
      if ( stretch.dword  >= 0x100     ) byte_val |= 0x04;
    }

    found.buffer[found.offset++] =  byte_val;

    if ( verbosity > 1 )
      fprintf(stderr, "block %i  control %x  stretch %i  to %i  count %i  from %i  to.offset %i\n",
              block, byte_val, stretch,
              found.pair[block].to,
              found.pair[block].count,
              found.pair[block].from,
              to.index.natural[found.pair[block].to.dword]);

    if ( origin.dword  >=  0x1000000 ) found.buffer[found.offset++] = origin.byte.b3;
    if ( origin.dword  >=  0x10000   ) found.buffer[found.offset++] = origin.byte.b2;
    if ( origin.dword  >=  0x100     ) found.buffer[found.offset++] = origin.byte.b1;
                                       found.buffer[found.offset++] = origin.byte.b0;

    if ( size.dword    >=  0x1000000 ) found.buffer[found.offset++] = size.byte.b3;
    if ( size.dword    >=  0x10000   ) found.buffer[found.offset++] = size.byte.b2;
    if ( size.dword    >=  0x100     ) found.buffer[found.offset++] = size.byte.b1;
                                       found.buffer[found.offset++] = size.byte.b0;

    if (   stretch.dword  >   0 ) {
      if ( stretch.dword  >=  0x1000000 ) found.buffer[found.offset++] = stretch.byte.b3;
      if ( stretch.dword  >=  0x10000   ) found.buffer[found.offset++] = stretch.byte.b2;
      if ( stretch.dword  >=  0x100     ) found.buffer[found.offset++] = stretch.byte.b1;
                                          found.buffer[found.offset++] = stretch.byte.b0;
    }
  }

         unmatched_size.dword   =  0;
  offset_unmatched_size         =  found.offset;
  found.offset                 +=  4;
     to.offset                  =  0;

  for ( block = 0; block < found.count ; block++ ) {
    stretch.dword    =  to.index.natural[found.pair[block].to.dword]  -  
                        to.offset;

    if  ( stretch.dword > 0 ) {
      memcpy ( found.buffer + found.offset,
                  to.buffer +    to.offset, stretch.dword );

      unmatched_size.dword  += stretch.dword;
      found.offset          += stretch.dword;
    }
    to.offset     = to.index.natural[found.pair[block].to.dword +
                                     found.pair[block].count.dword ];
  }

  found.buffer[offset_unmatched_size++] = unmatched_size.byte.b3;
  found.buffer[offset_unmatched_size++] = unmatched_size.byte.b2;
  found.buffer[offset_unmatched_size++] = unmatched_size.byte.b1;
  found.buffer[offset_unmatched_size++] = unmatched_size.byte.b0;

  SHA1_Init(&ctx);
  SHA1_Update(&ctx, found.buffer + 4 + DIGEST_SIZE, found.offset - (4 + DIGEST_SIZE));
  SHA1_Final(found.buffer + 4, &ctx);

  fwrite( found.buffer, 1, found.offset, stdout );
/*
  free(found.buffer);
*/
}


void  make_sdelta(INPUT_BUF *from_ibuf, INPUT_BUF *to_ibuf)  {
  FROM			from;
  TO			to;
  MATCH			match;
  FOUND			found;
  unsigned int		count, line, total, where, start, finish, limit;
  u_int16_t		tag;
  QWORD			crc, fcrc;
  SHA_CTX		ctx;

  from.buffer = from_ibuf->buf;
  to.buffer   =   to_ibuf->buf;
  from.size   = from_ibuf->size;
  to.size     =   to_ibuf->size;

  make_index  ( &from.index, from.buffer, from.size );
  to.index.natural  =  natural_block_list ( to.buffer, to.size, &to.index.naturals );
  to.index.crc      =  crc_list ( to.buffer, to.index.natural, to.index.naturals );

  SHA1_Init(&ctx);
  SHA1_Update(&ctx, from.buffer, from.size);
  SHA1_Final(from.digest, &ctx);

/*
  found.pair        =  malloc ( sizeof(PAIR) * to.index.naturals >> 2 );
*/
  found.pair        = ( PAIR * ) to.index.crc;

  found.count       =  0;
  to.block          =  0;

  to.index.ordereds = to.index.naturals - 1;
  while  ( to.index.ordereds  >  to.block )  {

    if   ( ( to.index.natural[to.block + 2] -
             to.index.natural[to.block    ] ) >= 0x10 ) {

      tag            =  qtag( crc = *(QWORD *)( to.index.crc + to.block ) );
      start          =  from.index.tags[tag].range;

      if  ( start == 0 )  {  to.block++;  continue;  }

      where  =  from.index.tags[tag].index;
      finish =  where + start;
      start--;

      while ( start >= 0x10000 )
      leap(0x10000);
      leap(0x8000);
      leap(0x4000);
      leap(0x2000);
      leap(0x1000);
      leap(0x800);
      leap(0x400);
      leap(0x200);
      leap(0x100);
      leap(0x80);
      leap(0x40);
      leap(0x20);
      leap(0x10);
      leap(0x8);
      leap(0x4);
      leap(0x2);
      leap(0x1);

      match.count    =  1;
         to.limit    =  to.index.naturals - to.block;

      from.block  =  from.index.ordered[where]; 
      fcrc        =  *(QWORD *)( from.index.crc + from.block );

      while ( ( where       <   finish                )  &&
              ( crc.qword   >   fcrc.qword            )  &&
              ( ++where     <   from.index.ordereds   )
            )
      {  from.block  =  from.index.ordered[where];
         fcrc        =  *(QWORD *)( from.index.crc + from.block );
      }

      while ( crc.qword  ==  fcrc.qword ) {

        count       =  2;
        from.limit  =  from.index.naturals - from.block;

        if ( to.limit > from.limit )
          limit  =      from.limit;  else
          limit  =        to.limit;

        if ( ( count > match.count ) ||
             ( from.index.crc[from.block + match.count].dword ==
                 to.index.crc[  to.block + match.count].dword ) )
          while  ( ( count < limit )  &&
                   ( from.index.crc[from.block + count].dword ==
                       to.index.crc[  to.block + count].dword ) )
            count++;

        while ( count > match.count ) {
          if ( memcmp( from.index.natural[from.block] + from.buffer,
                        to.index.natural[   to.block] +   to.buffer,
                        to.index.natural[   to.block + count] -
                        to.index.natural[   to.block        ]
                       ) == 0 ) {
            match.line   =  from.block;
            match.count  =  count;
            count  =  0;
          } else  count--;
        }

        if ( ++where  < from.index.ordereds ) {
          from.block  = from.index.ordered[where];
          fcrc        =  *(QWORD *)( from.index.crc + from.block );
        } else break;

      }  /* finished finding matches for to.block */

      if ( match.count > 1 ) {
        found.pair[found.count].to.dword      =     to.block;
        found.pair[found.count].from.dword    =  match.line;
        found.pair[found.count].count.dword   =  match.count;
        to.block                             +=  match.count;
        found.count++;
      } else  to.block++;
    } else    to.block++;
  }

  found.pair[found.count  ].to.dword     =  to.index.naturals;
  found.pair[found.count  ].from.dword   =  0;
  found.pair[found.count++].count.dword  =  0;
/*
  found.pair                             =  realloc ( found.pair, sizeof(PAIR) * found.count );
*/
  temp.current = (void *) to.index.crc + sizeof(PAIR) * found.count;

  if ( verbosity > 0 ) {
    fprintf(stderr, "Statistics for sdelta generation.\n");

    fprintf(stderr, "Blocks in from                      %i\n", from.index.naturals);
    fprintf(stderr, "Blocks in to                        %i\n",   to.index.naturals);
    fprintf(stderr, "Sorted blocks in from               %i\n", from.index.ordereds);

    total=0;
    for ( where = 0; where < found.count; where++)
      total += found.pair[where].count.dword;
    fprintf(stderr, "Matching blocks                     %i\n", total);
    fprintf(stderr, "Umatched blocks                     %i\n", to.index.naturals - total);

    fprintf(stderr, "Matching blocks sequences           %i\n", found.count);

    total=0;
    for ( where = 0; where < found.count; where++)
      if (found.pair[where].count.dword > 1 )  total++;
    fprintf(stderr, "Matches with consecutive sequences  %i\n", total);

    total=0;
    for ( where = 0; where < found.count - 1; where++)
      if  ( ( found.pair[where].to.dword +
              found.pair[where].count.dword )  ==
              found.pair[where+1].to.dword  )
        total++;
    fprintf(stderr, "Adjacent matching blocks sequences  %i\n", total);

    total=0;
    for ( where = 0; where < found.count; where++) {
      line = found.pair[where].to.dword; 
      for ( count = 0 ; count < found.pair[where].count.dword ; count++) {
       total += to.index.natural[line + 1] -
                to.index.natural[line    ];
       line++;
      }
    }

    fprintf(stderr, "Bytes in to                         %i\n", to.size);
    fprintf(stderr, "Bytes matched                       %i\n", total);
    fprintf(stderr, "Bytes unmatched                     %i\n", to.size - total);
  }

/*
  free   (  from.index.natural     );
  free   (  from.index.crc         );
  free   (  from.index.ordered     );
*/

  unload_buf(from_ibuf);

/*
  fprintf(stderr, "total %i  size %i\n", temp.size,
          temp.current - temp.start );
*/

  output_sdelta(found, to, from);

/*
  unload_buf(to_ibuf);
  free   (    to.index.natural     );
  free   (    to.index.crc         );
  free   ( found.pair              );
*/

}


void   make_to(INPUT_BUF *from_ibuf, INPUT_BUF *found_ibuf)  {
  FOUND			found;
  FROM			from, delta;
  TO			to;
  DWORD			*dwp, stretch;
  unsigned char		control;
  u_int32_t		line;
  u_int32_t		block;
  u_int32_t		size;
  SHA_CTX		ctx;

  if (from_ibuf) {
      from.buffer  =  from_ibuf->buf;
      from.size    =  from_ibuf->size;
  }
  else {
      from.buffer  =  NULL;
      from.size    =  0;
  }
  
  found.buffer  =  found_ibuf->buf;
  found.size    =  found_ibuf->size;

  if  ( memcmp(found.buffer, magic, 4) != 0 ) {
    fprintf(stderr, "Input on stdin did not start with sdelta magic.\n");
    fprintf(stderr, "Hint: cat sdelta_file from_file | sdelta  > to_file\n");
    exit(EXIT_FAILURE);
  }

  found.offset       =  4 + 2 * DIGEST_SIZE;  /* Skip the magic and 2 sha1 */
  dwp                =  (DWORD *)&to.size;
  dwp->byte.b3       =  found.buffer[found.offset++];
  dwp->byte.b2       =  found.buffer[found.offset++];
  dwp->byte.b1       =  found.buffer[found.offset++];
  dwp->byte.b0       =  found.buffer[found.offset++];
  /* in rare cases found.size may not be enough */
/*
  found.pair         =  malloc ( found.size );
*/

  found.pair         = (PAIR *) temp.current;
  found.count        =  0;
  line               =  0;

  size          =  1;
  while ( size !=  0 ) {

    control     =  found.buffer[found.offset++];

    found.pair[found.count].from.dword  =  0;

    switch ( control & 0xc0 ) {
      case 0xc0 : found.pair[found.count].from.byte.b3 = found.buffer[found.offset++];
                  found.pair[found.count].from.byte.b2 = found.buffer[found.offset++];
                  found.pair[found.count].from.byte.b1 = found.buffer[found.offset++];
                  found.pair[found.count].from.byte.b0 = found.buffer[found.offset++];
                  break;
      case 0x80:  found.pair[found.count].from.byte.b2 = found.buffer[found.offset++];
                  found.pair[found.count].from.byte.b1 = found.buffer[found.offset++];
                  found.pair[found.count].from.byte.b0 = found.buffer[found.offset++];
                  break;
      case 0x40:  found.pair[found.count].from.byte.b1 = found.buffer[found.offset++];
                  found.pair[found.count].from.byte.b0 = found.buffer[found.offset++];
                  break;
      default:    found.pair[found.count].from.byte.b0 = found.buffer[found.offset++];
    }

    found.pair[found.count].count.dword  =  0;
    switch ( control & 0x30 ) {
      case 0x30 : found.pair[found.count].count.byte.b3 = found.buffer[found.offset++];
                  found.pair[found.count].count.byte.b2 = found.buffer[found.offset++];
                  found.pair[found.count].count.byte.b1 = found.buffer[found.offset++];
                  found.pair[found.count].count.byte.b0 = found.buffer[found.offset++];
                  break;
      case 0x20:  found.pair[found.count].count.byte.b2 = found.buffer[found.offset++];
                  found.pair[found.count].count.byte.b1 = found.buffer[found.offset++];
                  found.pair[found.count].count.byte.b0 = found.buffer[found.offset++];
                  break;
      case 0x10:  found.pair[found.count].count.byte.b1 = found.buffer[found.offset++];
                  found.pair[found.count].count.byte.b0 = found.buffer[found.offset++];
                  break;
      default:    found.pair[found.count].count.byte.b0 = found.buffer[found.offset++];
    }

    size  =  found.pair[found.count].count.dword;

    if  ( ( control & 2 )  == 2 ) {
      stretch.dword  =  0;
      switch ( control & 0x0c ) {
        case 0x0c : stretch.byte.b3 = found.buffer[found.offset++];
                    stretch.byte.b2 = found.buffer[found.offset++];
                    stretch.byte.b1 = found.buffer[found.offset++];
                    stretch.byte.b0 = found.buffer[found.offset++];
                    break;
        case 0x08:  stretch.byte.b2 = found.buffer[found.offset++];
                    stretch.byte.b1 = found.buffer[found.offset++];
                    stretch.byte.b0 = found.buffer[found.offset++];
                    break;
        case 0x04:  stretch.byte.b1 = found.buffer[found.offset++];
                    stretch.byte.b0 = found.buffer[found.offset++];
                    break;
        default:    stretch.byte.b0 = found.buffer[found.offset++];
      }
      line += stretch.dword;
    }

    if ( verbosity > 1 )
      fprintf(stderr, "block %i  control %x  stretch %i  to %i  count %i  from %i\n",
              found.count,
              control,
              stretch,
              line,
              found.pair[found.count].count,
              found.pair[found.count].from);

            found.pair[found.count  ].to.dword  = line;
    line += found.pair[found.count++].count.dword;

  };

/*
  found.pair    =  realloc( found.pair, sizeof(PAIR) * found.count );
*/
  temp.current += sizeof(PAIR) * found.count;

  dwp           =  (DWORD *)&delta.size;
  dwp->byte.b3  =  found.buffer[found.offset++];
  dwp->byte.b2  =  found.buffer[found.offset++];
  dwp->byte.b1  =  found.buffer[found.offset++];
  dwp->byte.b0  =  found.buffer[found.offset++];

  delta.buffer  =  found.buffer + found.offset;
  if  ( from.buffer == NULL )  {
           from.buffer  =  found.buffer + found.offset + delta.size;
           from.size    =  found.size   - found.offset - delta.size;
          found.size    =  found.size   - from.size    - (4 + DIGEST_SIZE);
  } else  found.size   -=  4 + DIGEST_SIZE;

  SHA1_Init(&ctx);
  SHA1_Update(&ctx, from.buffer, from.size);
  SHA1_Final(from.digest, &ctx);
  
  SHA1_Init(&ctx);
  SHA1_Update(&ctx, found.buffer + 4 + DIGEST_SIZE, found.size);
  SHA1_Final(found.digest, &ctx);
	    
  if  ( memcmp( found.digest, found.buffer + 4, DIGEST_SIZE ) != 0 ) {
    fprintf(stderr, "The sha1 for this sdelta did not match.\nAborting.\n");
    exit(EXIT_FAILURE);
  }

  if  ( memcmp( from.digest, found.buffer + DIGEST_SIZE + 4, DIGEST_SIZE ) != 0 ) {
    fprintf(stderr, "The sha1 for the dictionary file did not match.\nAborting.\n");
    exit(EXIT_FAILURE);
  }

   from.index.natural  =  natural_block_list (  from.buffer,  from.size,  &from.index.naturals );
  delta.index.natural  =  natural_block_list ( delta.buffer, delta.size, &delta.index.naturals );
  delta.offset  =  0;
   from.offset  =  0;
     to.block   =  0;
  delta.block   =  0;

  for ( block = 0; block < found.count; block++ ) {
    stretch.dword = found.pair[block].to.dword - to.block;
    if ( stretch.dword > 0 ) {
      size  = delta.index.natural[delta.block + stretch.dword] -
              delta.index.natural[delta.block                ];
      write( 1, delta.buffer + delta.offset, size );
      delta.offset += size;
      delta.block  += stretch.dword;
         to.block  += stretch.dword;
    }

    size = from.index.natural[found.pair[block].from.dword + 
                              found.pair[block].count.dword ] -
           from.index.natural[found.pair[block].from.dword  ];
    from.offset = from.index.natural[ found.pair[block].from.dword ];
    write( 1, from.buffer + from.offset, size );
    to.block  += found.pair[block].count.dword;
  }

  if (from_ibuf)
      unload_buf(from_ibuf);
  unload_buf(found_ibuf);
}


void  help(void)  {
  printf("\nsdelta designed programmed and copyrighted by\n");
  printf("Kyle Sallee in 2004, 2005, All Rights Reserved.\n");
  printf("sdelta is distributed under the Sorcerer Public License version 1.1\n");
  printf("Please read /usr/doc/sdelta/LICENSE\n\n");

  printf("sdelta records the differences between source tarballs.\n");
  printf("First, sdelta can make a delta patch between two files.\n");
  printf("Then,  sdelta can make the second file when given both\n");
  printf("the previously generated delta file and the first file.\n\n");

  printf("Below is an example to make a bzip2 compressed sdelta patch file.\n\n");
  printf("$ sdelta linux-2.6.7.tar linux-2.6.8.1.tar > linux-2.6.7-2.6.8.1.tar.sd\n");
  printf("$ bzip2  linux-2.6.7-2.6.8.1.tar.sd\n\n\n");
  printf("Below is an example for making linux-2.6.8.1.tar\n\n");
  printf("$ bunzip2 linux-2.6.7-2.6.8.1.tar.sd.bz2\n");
  printf("$ sdelta  linux-2.6.7.tar linux-2.6.7-2.6.8.1.tar.sd > linux-2.6.8.1.tar\n");
  exit(EXIT_FAILURE);
}


void  parse_parameters( char *f1, char *f2)  {
  INPUT_BUF b1, b2;

  load_buf(f1, &b1);
  load_buf(f2, &b2);

  if ( memcmp( b2.buf, magic, 4 ) == 0 ) {
    init_temp(MAX(b1.size, b2.size));    make_to     (&b1, &b2); } else {
    init_temp(MAX(b1.size, b2.size)*2);  make_sdelta (&b1, &b2); }

}


void  parse_stdin(void) {
  INPUT_BUF b;

  load_buf(NULL, &b);  init_temp(b.size);
  make_to (NULL, &b);
} 


int	main	(int argc, char **argv)  {

  if  ( NULL !=  getenv("SDELTA_VERBOSE") )
    sscanf(      getenv("SDELTA_VERBOSE"), "%i", &verbosity );  

  switch (argc) {
    case  3 :  parse_parameters(argv[1], argv[2]);  break;
    case  1 :  parse_stdin();                       break;
    default :  help();                              break;
  }
  exit(EXIT_SUCCESS);
}
