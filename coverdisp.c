#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <pthread.h>

#include <mpd/client.h>

#define RED "\x1b[31;01m"
#define GREEN "\x1b[32;06m"
#define RESET "\x1b[0m"

#define MPD_ROOT_DIRECTORY "/home/lain/Music/beets/"

int error(const char* msg) {
    printf(RED"Error: "RESET"%s\n", msg);
    return 1;
}

int copy(const char* source, const char* dest) {
	int inFile = open(source, O_RDONLY);
	int outFile = open(dest, O_WRONLY);

	if (inFile < 0 || outFile < 0) {
		return 1;
	}

	char buf[8192];
	ssize_t result;
	while ((result = read(inFile, &buf[0], sizeof(buf)))) {
		write(outFile, &buf[0], (size_t)result);
	};

	close(inFile);
	close(outFile);
}

void getFirstWord(char* word, char* out) {
    char* space = strstr(word, " ");
    if (space) {
        size_t idx = (size_t)(space - word);
        strncpy(out, word, idx);
        out[idx] = '\0';
    } else {
        strcpy(out, word);
    }
}

int getSong(struct mpd_connection* conn, char* title, char* artist, char* album, char* directory) {
    struct mpd_song* song;

    mpd_command_list_begin(conn, true);
    mpd_send_current_song(conn);
    mpd_command_list_end(conn);

    song = mpd_recv_song(conn);

    if (song == NULL) {
        // No song was playing,
        if (!mpd_response_finish(conn))
            return error("Failed to close response");

        return 1;
    }

    const char* tmpTitle = mpd_song_get_tag(song, MPD_TAG_TITLE, 0);
    const char* tmpArtist = mpd_song_get_tag(song, MPD_TAG_ALBUM_ARTIST, 0);
    const char* tmpAlbum = mpd_song_get_tag(song, MPD_TAG_ALBUM, 0);

    if (tmpTitle && tmpArtist && tmpAlbum) {
        strcpy(title, tmpTitle);
        strcpy(artist, tmpArtist);
        strcpy(album, tmpAlbum);
    } else {
        return error("Failed to get song tags");
    }

    mpd_song_free(song);

    if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS)
        return error(mpd_connection_get_error_message(conn));

    if (!mpd_response_finish(conn))
        return error("Failed to close response.");

    if (!mpd_send_list_meta(conn, artist))
        return error("Failed to query metadata");

    struct mpd_entity* entity;

    while ((entity = mpd_recv_entity(conn)) != NULL) {
        const struct mpd_directory* dir;
        if (mpd_entity_get_type(entity) == MPD_ENTITY_TYPE_DIRECTORY) {
            dir = mpd_entity_get_directory(entity);
            const char* path = mpd_directory_get_path(dir);
            char firstWord[32];
            getFirstWord(album, firstWord);
            if (strstr(path, firstWord)) {
                strcpy(directory, MPD_ROOT_DIRECTORY);
                strcat(directory, path);
            }
        }
        mpd_entity_free(entity);
    }

    if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS)
        return error(mpd_connection_get_error_message(conn));

    if (!mpd_response_finish(conn))
        return error("Failed to finish response");

    return 0;
}

void* update(void* unused) {
    struct mpd_connection* conn;
    conn = mpd_connection_new(NULL, 0, 30000);

    if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) {
        error("Could not establish connection with MPD");
        return NULL;
    }

    char lastAlbum[64];

    char title[64];
    char artist[64];
    char album[64];
    char path[128];

    do {
        if (!getSong(conn, title, artist, album, path)) {
            // Check if the album has changed
            if (strcmp(lastAlbum, album)) {
                strcpy(lastAlbum, album);

                strcat(path, "/cover.jpg");
                copy(path, "/tmp/cover.jpg");
            }
        }
    } while (!usleep(100000));

    mpd_connection_free(conn);
}

void* display(void* geometry) {
    // Create the file so feh doesn't fail
    system("touch /tmp/cover.jpg");

    // Call feh
    char cmdBuf[128];
    sprintf(cmdBuf, "feh -x -Z -Y -R 0.5 -g %s /tmp/cover.jpg", (char*)geometry);
    system(cmdBuf);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: coverdisp [geometry]\n");
        printf("   ex. coverdisp 800x800+50+140\n");
        exit(EXIT_FAILURE);
    }

    pthread_t displayThread;
    pthread_t updateThread;

    if (pthread_create(&displayThread, NULL, display, (void*)argv[1])) {
        printf(RED"Error: "RESET"pthread_create() failed");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&updateThread, NULL, update, NULL)) {
        printf(RED"Error: "RESET"pthread_create() failed");
        exit(EXIT_FAILURE);
    }

    pthread_join(displayThread, NULL);
    pthread_kill(updateThread, 0);

    return EXIT_SUCCESS;
}
