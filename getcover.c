/*
 * getcover: Extract picture data from audio files
 *
 * Copyright (c) 2015 Yasuyuki Suzuki
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * compile with: gcc -o getcover getcover.c
 *
 * Usage: %s [-v] [-o] [-f basename] path [path [path]...]
 *  -v: verbose mode
 *  -o: override mode. override image file even if it exists
 *  -f basename: specifiy name of image file without suffix
 *  path: path of the directory which contains FLAC or mp4 files
 *
 */

/*============================================================================
 Includes
 ============================================================================*/

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/*============================================================================
 Global variables, type definitions and prototypes
 ============================================================================*/

#define MAXPATHLEN 1000
#define DONE 1
#define NOTYET 0
int verbose = 0;
int override = 0;
int nbasename = 0;
char jpegFile[MAXPATHLEN];
char pngFile[MAXPATHLEN];

static void lookup(const char *arg);
static int get_FLAC_cover(FILE *fp, const char *dirpath);
static int get_m4a_cover(FILE *fp, const char *dirpath);


/*============================================================================
 Main Program
 ============================================================================*/

int main(int argc, char *argv[])
{
    int i,opt;
    
    while ((opt = getopt(argc, argv, "vof:")) != -1) {
        switch (opt) {
            case 'v':
                verbose = 1;
                break;
            case 'o':
                override = 1;
                break;
            case 'f':
                strcpy(jpegFile, optarg);
                strcat(jpegFile, ".jpg");
                strcpy(pngFile, optarg);
                strcat(pngFile, "png");
                nbasename = 2;
                break;
            default: /* '?' */
                fprintf(stderr, "Usage: %s [-v] [-o] [-f basename] path [path [path]...]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    
    /* if any path is not specified */
    if((argc - verbose - override - nbasename) < 2) {
        fprintf(stderr, "Usage: %s [-v] [-o] [-f basename] path [path [path]...]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    if (strlen(jpegFile) == 0) {
        strcpy(jpegFile, "Folder.jpg");
    }
    
    if (strlen(pngFile) == 0) {
        strcpy(pngFile, "Folder.png");
    }
    
    if(verbose) (void) printf("JPEG file name: %s\n", jpegFile);
    if(verbose) (void) printf("PNG file name: %s\n", pngFile);
    
    /* lookup all directory paths */
    for (i = 1 + verbose + override + nbasename; i < argc; i++)
        lookup(argv[i]);
}

/*============================================================================
 Function for looking up files in the designated directory
 ============================================================================*/

static void lookup(const char *arg)
{
    DIR *dirp;
    struct dirent *dp;
    FILE *fp;
    char str[10];
    char filepath[MAXPATHLEN];
    unsigned int offset;
    size_t size;
    
    if ((dirp = opendir(arg)) == NULL) {
        fprintf(stderr, "Could not open dir:%s\n", arg);
        perror("opendir()");
        return;
    }
    
    do {
        if ((dp = readdir(dirp)) != NULL)
            switch(dp->d_type) {
                case DT_DIR: /* directory */
                    if(verbose) (void) printf("DIR: %s\n", dp->d_name);
                    break;
                case DT_REG: /* regular file */
                    strcpy(filepath, arg);
                    strcat(filepath, "/");
                    strcat(filepath, dp->d_name);
                    if((fp = fopen(filepath, "r")) != NULL) {
                        size = fread(str, 1, 4, fp);
                        if(memcmp(str,"fLaC", 4)==0) { /* If FLAC, get cover */
                            if(verbose) (void) printf("FLAC FILE: %s\n", dp->d_name);
                            if(get_FLAC_cover(fp, arg) == DONE)
                                return;
                        }
                        else if(memcmp(str,"RIFF", 4)==0) {
                            if(verbose) (void) printf("WAV FILE: %s\n", dp->d_name);
                        }
                        else if(memcmp(str,"ID3\3", 4)==0) {
                            if(verbose) (void) printf("mp3 FILE: %s\n", dp->d_name);
                        }
                        else {
                            offset = ((str[0]<<24)|(str[1]<<16)|(str[2]<<8)|str[3]);
                            size = fread(str, 1, 4, fp);
                            if(memcmp(str,"ftyp", 4)==0) {
                                if(verbose) (void) printf("MP4 FILE: %s\n", dp->d_name);
                                if(get_m4a_cover(fp, arg) == DONE)
                                    return;
                            }
                            else {
                                if(verbose) (void) printf("Other FILE: %s\n", dp->d_name);
                            }
                        }
                        fclose(fp);
                        break;
                    default:
                        if(verbose) (void) printf("Special file: %s\n", dp->d_name);
                    } else {
                        perror("lookup()::fopen()");
                    }
            }
    } while (dp != NULL);
    
    (void) closedir(dirp);
    return;
}

/*============================================================================
 Function for extracting JPEG/PNG data from a FLAC file
 ============================================================================*/

static int get_FLAC_cover(FILE *fp, const char *dirpath)
{
    unsigned char block_header;
    unsigned char length[3];
    unsigned int ilength;
    unsigned char buf[4];
    unsigned int picture_type;
    unsigned int mime_type_length;
    char *mime_type;
    unsigned int description_length;
    unsigned char *description;
    unsigned int picture_width;
    unsigned int picture_height;
    unsigned int color_depth;
    unsigned int number_of_indexed_color;
    unsigned int picture_length;
    char picture_path[MAXPATHLEN];
    unsigned char *picture_data;
    FILE *picture_fp;
    
    
    do {
        fread(&block_header, 1, 1, fp);
        fread(length, 1, 3, fp);
        ilength = (unsigned int) ((length[0]<<16)|(length[1]<<8)|(length[2]));
        switch(0x7F&block_header) {
            case 0:
                if(verbose) printf(" STREAMINFO\n");
                fseek(fp, ilength, SEEK_CUR);
                break;
            case 1:
                if(verbose) printf(" PADDING\n");
                fseek(fp, ilength, SEEK_CUR);
                break;
            case 2:
                if(verbose) printf(" APPLICATION\n");
                fseek(fp, ilength, SEEK_CUR);
                break;
            case 3:
                if(verbose) printf(" SEEKTABLE\n");
                fseek(fp, ilength, SEEK_CUR);
                break;
            case 4:
                if(verbose) printf(" VORBIS_COMMENT\n");
                fseek(fp, ilength, SEEK_CUR);
                break;
            case 5:
                if(verbose) printf(" CUESHEET\n");
                fseek(fp, ilength, SEEK_CUR);
                break;
            case 6: /* Picture data */
                if(verbose) printf(" PICTURE\n");
                fread(buf, 1, 4, fp);
                picture_type = ((buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3]);
                /* if Picture type is 3, this is Front Cover picture */
                if(verbose) printf("  Picture type = %d\n", picture_type);
                fread(buf, 1, 4, fp);
                mime_type_length = ((buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3]);
                if(verbose) printf("  MIME type length = %d\n", mime_type_length);
                
                mime_type = (char *) malloc(mime_type_length + 1);
                fread(mime_type, 1, mime_type_length, fp);
                mime_type[mime_type_length] = '\0';
                if(verbose) printf("  MIME type = %s\n", mime_type);
                
                fread(buf, 1, 4, fp);
                description_length = ((buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3]);
                if(verbose) printf("  Description length = %d\n", description_length);
                
                description = (unsigned char *) malloc(description_length);
                fread(mime_type, 1, description_length, fp);
                if(verbose) printf("  Description = %s\n", description);
                free(description);
                
                fread(buf, 1, 4, fp);
                picture_width = ((buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3]);
                if(verbose) printf("  Picture width = %d\n", picture_width);
                
                fread(buf, 1, 4, fp);
                picture_height = ((buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3]);
                if(verbose) printf("  Picture height = %d\n", picture_height);
                
                fread(buf, 1, 4, fp);
                color_depth = ((buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3]);
                if(verbose) printf("  Color Depth = %d\n", color_depth);
                
                fread(buf, 1, 4, fp);
                number_of_indexed_color = ((buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3]);
                if(verbose) printf("  Number of colors for indexed-color = %d\n", number_of_indexed_color);
                fread(buf, 1, 4, fp);
                
                picture_length = ((buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3]);
                if(verbose) printf("  Length of picture data = %d\n", picture_length);
                
                strcpy(picture_path, dirpath);
                strcat(picture_path, "/");
                if(strcmp(mime_type, "image/png") == 0) {
                    strcat(picture_path, pngFile);
                    fprintf(stderr, "  Warning: Image type is PNG not JPEG, generating Folder.png\n");
                }
                else if(strcmp(mime_type, "image/jpeg") == 0)
                    strcat(picture_path, jpegFile);
                else {
                    strcat(picture_path, jpegFile);
                    fprintf(stderr, "  Warning: unknown MIME type:%s, generating %s\n", mime_type, jpegFile);
                }
                /* check if cover-art file already exists */
                if((picture_fp = fopen(picture_path, "r")) != NULL) {
                    
                    if(verbose) printf("  File %s exists, did not override.\n", picture_path);
                    if(!override) {
                        fclose(picture_fp);
                        free(mime_type);
                        return DONE;
                    }
                }
                /* some ripper software create blank PICTURE frame :-( */
                if(picture_length == 0) {
                    fprintf(stderr, "  Warning: picture size is 0, will not create JPEG file.\n");
                    break;
                }
                /* read picture date and write it to a file */
                picture_data = malloc(picture_length);
                fread(picture_data, 1, picture_length, fp);
                picture_fp = fopen(picture_path, "w");
                if(picture_fp == NULL) {
                    perror("get_FLAC_cover()::fopen()");
                    return DONE;
                }
                fwrite(picture_data, 1, picture_length, picture_fp);
                if(verbose) printf("  Wrote picture to %s\n", picture_path);
                fclose(picture_fp);
                free(picture_data);
                free(mime_type);
                return DONE;
                
            default:
                if(verbose) printf(" other metadata\n");
                fseek(fp, ilength, SEEK_CUR);
        }
    } while ((0x80&block_header) == 0);
    
    return NOTYET;
}


/*============================================================================
 Function for extracting JPEG/PNG data from a m4a file
 ============================================================================*/
static int get_m4a_cover(FILE *fp, const char *dirpath)
{
    unsigned char coffset[5], boxtype[5];
    unsigned int offset1, offset2, offset3, offset4, offset5, covrdata_len;
    unsigned int sum2, sum3, sum4, sum5;
    int flag;
    size_t size;
    char picture_path[MAXPATHLEN];
    char flag_data[8];
    unsigned char *picture_data;
    FILE *picture_fp;
    
    rewind(fp);
    while((size = fread(coffset, 1, 4, fp))>0) {
        offset1 = ((coffset[0]<<24)|(coffset[1]<<16)|(coffset[2]<<8)|coffset[3]);
        size = fread(boxtype, 1, 4, fp);
        boxtype[4] = 0;
        if(memcmp(boxtype,"moov",4)==0) { /* look into moov box */
            sum2 = 0;
            do {
                size = fread(coffset, 1, 4, fp);
                offset2 = ((coffset[0]<<24)|(coffset[1]<<16)|(coffset[2]<<8)|coffset[3]);
                sum2 += offset2;
                size = fread(boxtype, 1, 4, fp);
                boxtype[4] = 0;
                if(verbose) printf(" box type = moov:%s\n", boxtype);
                if(memcmp(boxtype, "udta", 4)==0) { /* look into udta box */
                    sum3 = 0;
                    do {
                        size = fread(coffset, 1, 4, fp);
                        offset3 = ((coffset[0]<<24)|(coffset[1]<<16)|(coffset[2]<<8)|coffset[3]);
                        sum3 += offset3;
                        size = fread(boxtype, 1, 4, fp);
                        boxtype[4] = 0;
                        if(verbose) printf(" box type = moov:udta:%s\n", boxtype);
                        if(memcmp(boxtype, "meta", 4)==0) { /* look into meta */
                            sum4 = 0;
                            do {
                                size = fread(coffset, 1, 4, fp);
                                offset4 = ((coffset[0]<<24)|(coffset[1]<<16)|(coffset[2]<<8)|coffset[3]);
                                if(offset4 == 0) { /* one byte atom version */
                                    size = fread(coffset, 1, 4, fp);
                                    offset4 = ((coffset[0]<<24)|(coffset[1]<<16)|(coffset[2]<<8)|coffset[3]);
                                }
                                size = fread(boxtype, 1, 4, fp);
                                boxtype[4] = 0;
                                sum4 += offset4;
                                if(verbose) printf(" box type = moov:udta:meta:%s\n", boxtype);
                                if(memcmp(boxtype, "ilst", 4)==0) { /* look into ilst */
                                    do {
                                        sum5 = 0;
                                        size = fread(coffset, 1, 4, fp);
                                        offset5 = ((coffset[0]<<24)|(coffset[1]<<16)|(coffset[2]<<8)|coffset[3]);
                                        if(offset5 == 0) { /* one byte atom version */
                                            size = fread(coffset, 1, 4, fp);
                                            offset5 = ((coffset[0]<<24)|(coffset[1]<<16)|(coffset[2]<<8)|coffset[3]);
                                        }
                                        size = fread(boxtype, 1, 4, fp);
                                        boxtype[4] = 0;
                                        sum5 += offset5;
                                        if(verbose) printf(" box type = moov:udta:meta:ilst:%s\n", boxtype);
                                        if(memcmp(boxtype, "covr", 4)==0) { /* look into cover */
                                            size = fread(coffset, 1, 4, fp);
                                            covrdata_len = ((coffset[0]<<24)|(coffset[1]<<16)|(coffset[2]<<8)|coffset[3]);
                                            size = fread(boxtype, 1, 4, fp);
                                            boxtype[4] = 0;
                                            if(verbose) printf(" box type = moov:udta:meta:ilst:covr:%s\n", boxtype);
                                            
                                            strcpy(picture_path, dirpath);
                                            strcat(picture_path, "/");
                                            size = fread(flag_data, 1, 8, fp);
                                            flag = flag_data[3];
                                            if(flag == 14) {
                                                strcat(picture_path, pngFile);
                                                fprintf(stderr, "  Warning: Image type is PNG not JPEG, generating Folder.png\n");
                                            }
                                            else if(flag == 13)
                                                strcat(picture_path, jpegFile);
                                            else {
                                                strcat(picture_path, jpegFile);
                                                fprintf(stderr, "  Warning: unknown image format:%d, generating %s\n", flag, jpegFile);
                                            }
                                            /* check if cover-ar file already exists */
                                            if((picture_fp = fopen(picture_path, "r")) != NULL) {
                                                if(verbose) printf("  File %s exists, did not override.\n", picture_path);
                                                if(!override) {
                                                    fclose(picture_fp);
                                                    return DONE;
                                                }
                                            }
                                            /* some ripper software create blank PICTURE frame :-( */
                                            if(covrdata_len == 0) {
                                                fprintf(stderr, "  Warning: picture size is 0, will not create JPEG file.\n");
                                                return NOTYET;
                                            }
                                            /* read picture date and write it to a file */
                                            picture_data = malloc(covrdata_len);
                                            fread(picture_data, 1, covrdata_len-8, fp);
                                            picture_fp = fopen(picture_path, "w");
                                            if(picture_fp == NULL) {
                                                perror("get_m4a_cover()::fopen()");
                                                return DONE;
                                            }
                                            fwrite(picture_data, 1, covrdata_len, picture_fp);
                                            if(verbose) printf("  Wrote picture to %s\n", picture_path);
                                            fclose(picture_fp);
                                            free(picture_data);
                                            return DONE;
                                        } /* end of cover search */
                                        fseek(fp, offset5-8, SEEK_CUR);
                                    } while(sum5 < offset4);
                                    return NOTYET; /* cover not found in ilst */
                                } /* end of ilst search */
                                fseek(fp, offset4-8, SEEK_CUR);
                            } while(sum4 < offset3);
                        } /* end of meta search */
                        fseek(fp, offset3-8, SEEK_CUR);
                    } while(sum3 < offset2);
                } /* end of udta search */
                else fseek(fp, offset2-8, SEEK_CUR);
            } while(sum2 < offset1);
        } /* end of moov search */
        else fseek(fp, offset1-8, SEEK_CUR);
    }
    return NOTYET;
}
