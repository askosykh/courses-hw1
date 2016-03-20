#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>


// one object per one word. We need this struct for fast search of words
struct word_in_dictionary {
    // pointer to the c-string contains the word
    const char *word;
    // number of last file with this word
    size_t last_file;
    // index of element in array of words and counts
    size_t index;
};

// one object per word. We need this struct for fast sort when we will know count of every word
struct count_of_word {
    // pointer to the c-string contains the word
    const char *word;
    // counter of files whith this word
    size_t count;
};

// one object per letter
struct page_in_dictionary {
    // array of words, started whith same letter
    struct word_in_dictionary *words;
    // count of words in array above
    size_t allocated_words;
    // count of words for what memory is allocated
    size_t used_words;
};

// increase quant of memory
#define STRUCT_COUNT_OF_WORD_INCREASE_QUANT (128)
#define STRUCT_WORD_IN_DICTIONARY_INCREASE_QUANT (16)

// array of pages -- one page per letter 26*2 english letters, 33*2 russian letters and one for other letters (just in case)
// ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz
// ЁёАБВГДЕЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдежзийклмнопрстуфхцчшщъыьэюя
struct page_in_dictionary dictionaty[119];
size_t last_page = sizeof(dictionaty) / sizeof(struct page_in_dictionary) - 1;
struct count_of_word *words_and_counts = NULL;
size_t words_and_counts_allocated = 0;
size_t words_and_counts_used = 0;

// Ё = DO 81
// ё = D1 91
// А = D0 90 ;  п = D0 BF ;  р = D1 80 ;  я = D1 8F
static size_t get_russian_or_default_index( const unsigned char *letter ) {
    if ( !letter[1] || letter[0] < 0xD0 )
       return last_page;
    unsigned char first_letter = letter[0], second_letter = letter[1];
    if( first_letter == 0xD0 ) {
        if ( second_letter == 0x81 )
            return 52;
        if ( second_letter < 0x90 || second_letter > 0xBF )
            return last_page;
        return 54 + second_letter - 0x90;
    } else if ( first_letter == 0xD1 ) {
        if ( second_letter == 0x91 )
            return 53;
        if ( second_letter < 0x80 || second_letter > 0x8F )
            return last_page;
        return 102 + second_letter - 0x80;
    }
   return last_page;
}

static size_t get_index( const char *letter ) {
    if( *letter >= 0x41 && *letter < 0x5B )
        // english upper letter
        return *letter - 0x41;
    else if (*letter >= 0x61 && *letter < 0x7B )
        // english lower letter
        return *letter - 0x61 + 26;
    else
       return get_russian_or_default_index(letter);
}

static bool find_in_dict_and_increment(const char *word, size_t r, size_t file) {
    if (!word || !*word)
        return true;
    size_t index = get_index(word);
    // pointer to page with words starts with same letter
    struct page_in_dictionary *page = dictionaty + index;

    struct word_in_dictionary *word_in_dict = NULL;

    for( size_t i = 0; i < page->used_words; ++i ) {
        if ( !strncmp( page->words[i].word, word, r ) ) {
            word_in_dict = page->words + i;
            break;
        }
    }
    // if did not found
    if( !word_in_dict )
        return false;

    if( word_in_dict->last_file != file ) {
       // printf("word %.*s found in dict with new file\n", (int)r, word);
        words_and_counts[word_in_dict->index].count++;
        word_in_dict->last_file = file;
    }
    return true;
}

static bool put_to_dict( const char *word, size_t file_number ) {
    // reallocate mamory if need
    if( words_and_counts == words_and_counts ) {
        size_t new_allocated = words_and_counts_allocated + STRUCT_COUNT_OF_WORD_INCREASE_QUANT;
        size_t new_size = new_allocated * sizeof(struct count_of_word);
        words_and_counts = (struct count_of_word*)realloc( words_and_counts, new_size );
        words_and_counts_allocated = new_allocated;
    }
    // write new word to the elemant after last used
    words_and_counts[words_and_counts_used].word = word;
    words_and_counts[words_and_counts_used].count = 1;
    words_and_counts_used++;

    size_t index = get_index( word );
    if( index >= sizeof(dictionaty) )
        exit(2);

    // pointer to page with words starts with same letter
    struct page_in_dictionary *page = dictionaty + index;
    // realocate memory if need
    if( page->allocated_words == page->used_words ) {
        size_t new_allocated =  page->allocated_words + STRUCT_WORD_IN_DICTIONARY_INCREASE_QUANT;
        size_t new_size = new_allocated * sizeof(struct word_in_dictionary);
        page->words = (struct word_in_dictionary*)realloc( page->words, new_size );
        page->allocated_words = new_allocated;
    }
    // write new word to the element after last used
    struct word_in_dictionary *word_in_dict = page->words + page->used_words;
    word_in_dict->word = word;
    word_in_dict->index = words_and_counts_used - 1;
    word_in_dict->last_file = file_number;
    page->used_words++;
}

static size_t skip_space_and_punct(const char *buf, size_t len) {
    size_t i = 0;
    for(; i < len && (isspace(buf[i]) || ispunct(buf[i])); ++i);
    return i;
}

static size_t skip_not_space_and_punct(const char *buf, size_t len) {
    size_t i = 0;
    for(; i < len && (!isspace(buf[i]) && !ispunct(buf[i])); ++i);
    return i;
}

static size_t find_not_space_or_punct_from_end(const char *buf, size_t len) {
    for(size_t i = 1; i <= len; ++i) {
        if (!isspace(buf[len - i]) && !ispunct(buf[len - i]))
            return len - i + 1;
    }
    return 0;
}

static size_t find_space_or_punct_from_end(const char *buf, size_t len) {
    for(size_t i =  1; i <= len; ++i) {
        if (isspace(buf[len - i]) || ispunct(buf[len - i]))
            return len - i + 1;
    }
    return 0;
}

static void create_dict(const char *buf, size_t buf_len, size_t file) {
    size_t j = 0;
    size_t r = 0;
    while( j < buf_len  ) {
        r = skip_space_and_punct(buf + j, buf_len - j);
        j += r;
        if( j >= buf_len )
            break;
        const char *first_letter = buf + j;
        r = skip_not_space_and_punct(buf + j, buf_len - j);
        j += r;
        if(!find_in_dict_and_increment(first_letter, r, file)) {
            char *word = strndup(first_letter, r);
            put_to_dict(word, file);
        }
    }
}

//put to dict from file
static void work_with_file(size_t count_of_files, char** files) {
    size_t buf_len = 0;
    char buf[4096];
    char *buf_ptr = buf;
    size_t buf_size = sizeof(buf);
    for( size_t i = 0; i < count_of_files; ++i ) {
        char *fname = files[i];
        // printf("%s\n", fname);
        FILE *file = fopen(fname, "r");
        if ( !file )
            continue;
        while( buf_len = fread( buf_ptr, 1, buf_size, file) ) {
            size_t last_word_pos = buf_len;
            if ( buf_len == buf_size ) {
                last_word_pos = find_space_or_punct_from_end( buf_ptr, buf_len );
                if( !last_word_pos ) {
                    printf("too long word\n");
                    fclose(file);
                    exit(2);
                }
            }
            create_dict( buf, find_not_space_or_punct_from_end( buf_ptr, last_word_pos ) + buf_ptr - buf, i );
            size_t length_of_part = buf_len - last_word_pos;
            if( length_of_part )
                memcpy(buf, buf_ptr + last_word_pos, length_of_part);
            buf_size = sizeof(buf) - length_of_part;
            buf_ptr = buf + length_of_part;
        }
        if( buf != buf_ptr ) {
            create_dict( buf, buf_ptr - buf, i );
            buf_ptr = buf;
            buf_size = sizeof(buf);
        }
        //printf("%lu %s %lu\n", i, fname, words_and_counts_used);
        fclose(file);
    }

 
}
//comporator for qsort
static int structcmp_words_and_counts(const void *first, const void *second) {
    const struct count_of_word* left = (const struct count_of_word*)first;
    const struct count_of_word* right = (const struct count_of_word*)second;
    if (left->count < right->count)
        return 1;
    else
        if (right->count < left->count)
            return -1;
        else
            return strcmp(left->word,right->word);
}

int main( int argc, char** argv) {
    if( argc < 2 )
        return 1;
    size_t count_of_files = argc - 1;
    char **files = &argv[1]; 
    work_with_file(count_of_files, files);
    if( words_and_counts ) {
        qsort(words_and_counts,words_and_counts_used,sizeof(*words_and_counts),structcmp_words_and_counts);
        for( size_t i = 0; i < words_and_counts_used; ++i ){
            printf("%s %lu\n", words_and_counts[i].word, words_and_counts[i].count);
            free((void*)words_and_counts[i].word);
        }
        free( words_and_counts );
    }
    for ( size_t i = 0; i <= last_page; ++i )
        if( dictionaty[i].words )
            free( dictionaty[i].words );
    return 0;
}
