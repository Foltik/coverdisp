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
        *out[idx] = '\0';
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
    }
}

int getAlbumPath(struct mpd_connection* conn, char* artist, char** path) {
        char artistPath[128];
        strcpy(artistPath, lastArtist);

        if (!mpd_send_list_meta(conn, artistPath))
            return error("Failed to query song metadata");

        struct mpd_entity* entity;

        while ((entity = mpd_recv_entity(conn)) != NULL) {
            const struct mpd_song* song;
            const struct mpd_directory* dir;
            const struct mpd_playlist* pl;

            if (mpd_entity_get_type(entity) == MPD_ENTITY_TYPE_DIRECTORY) {
                dir = mpd_entity_get_directory(entity);
                const char* path = mpd_directory_get_path(dir);
                char firstWord[32];
                char* firstSpaceChar = strstr(lastAlbum, " ");
                if (firstSpaceChar) {
                    int firstSpace = firstSpaceChar - lastAlbum;
                    strncpy(firstWord, lastAlbum, firstSpace);
                    firstWord[firstSpace] = '\0';
                } else {
                    strcpy(firstWord, lastAlbum);
                }
                if (strstr(path, firstWord)) {
                    char copyBuf[128];
                    strcpy(copyBuf, MPD_ROOT_DIRECTORY);
                    strcat(copyBuf, path);
                    strcat(copyBuf, "/cover.jpg");
                    copy(copyBuf, "/tmp/cover.jpg");
                }
            }

            mpd_entity_free(entity);
        }

        if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) {
            printf(RED"Error: "RESET"%s\n", mpd_connection_get_error_message(conn));
            return 1;
        }
        if (!mpd_response_finish(conn)) {
            printf(RED"Error: "RESET"Failed to close response\n");
            return 1;
        }
}

int getMetadata(struct mpd_connection* conn, char** title, char** artist, char** album, char** path) {
    getSong(conn, title, artist, album);
    getAlbumPath(conn, *artist, path);
}

void* update(void* unused) {
    struct mpd_connection* conn;
    conn = mpd_connection_new(NULL, 0, 30000);

    if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) {
        printf(RED"Error: "RESET"Could not establish connection with MPD\n");
        return;
    }

    struct mpd_song* song;
    char lastAlbum[128];
    char lastArtist[64];
    lastAlbum[0] = '\0';

    do {
        mpd_command_list_begin(conn, true);
        mpd_send_current_song(conn);
        mpd_command_list_end(conn);

        song = mpd_recv_song(conn);

        if (song == NULL) {
            // No song playing, continue.
            if (!mpd_response_finish(conn)) {
                printf(RED"Error: "RESET"Failed to close response\n");
                return 1;
            }
            continue;
        }

        bool changed = false;
        const char* tmpAlbum = mpd_song_get_tag(song, MPD_TAG_ALBUM, 0);
        const char* tmpArtist = mpd_song_get_tag(song, MPD_TAG_ALBUM_ARTIST, 0);

        if (tmpAlbum && tmpArtist && strcmp(lastAlbum, tmpAlbum) != 0) {
            strcpy(lastAlbum, tmpAlbum);
            strcpy(lastArtist, tmpArtist);
            printf(GREEN"Album Changed: "RESET"%s - '%s'\n", tmpArtist, tmpAlbum);
            changed = true;
        }

        mpd_song_free(song);

        if (!mpd_response_finish(conn)) {
            printf(RED"Error: "RESET"Failed to close response\n");
            return 1;
        }

        if (!changed) continue;

        char artistPath[128];
        strcpy(artistPath, lastArtist);

        if (!mpd_send_list_meta(conn, artistPath)) {
            printf(RED"Error: "RESET"Failed to query song metadata\n");
            return 1;
        }

        struct mpd_entity* entity;

        while ((entity = mpd_recv_entity(conn)) != NULL) {
            const struct mpd_song* song;
            const struct mpd_directory* dir;
            const struct mpd_playlist* pl;

            if (mpd_entity_get_type(entity) == MPD_ENTITY_TYPE_DIRECTORY) {
                dir = mpd_entity_get_directory(entity);
                const char* path = mpd_directory_get_path(dir);
                char firstWord[32];
                char* firstSpaceChar = strstr(lastAlbum, " ");
                if (firstSpaceChar) {
                    int firstSpace = firstSpaceChar - lastAlbum;
                    strncpy(firstWord, lastAlbum, firstSpace);
                    firstWord[firstSpace] = '\0';
                } else {
                    strcpy(firstWord, lastAlbum);
                }
                if (strstr(path, firstWord)) {
                    char copyBuf[128];
                    strcpy(copyBuf, MPD_ROOT_DIRECTORY);
                    strcat(copyBuf, path);
                    strcat(copyBuf, "/cover.jpg");
                    copy(copyBuf, "/tmp/cover.jpg");
                }
            }

            mpd_entity_free(entity);
        }

        if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) {
            printf(RED"Error: "RESET"%s\n", mpd_connection_get_error_message(conn));
            return 1;
        }
        if (!mpd_response_finish(conn)) {
            printf(RED"Error: "RESET"Failed to close response\n");
            return 1;
        }

    } while (!usleep(100000));
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

    //return EXIT_SUCCESS;

    pid_t pid;

        system("touch /tmp/cover.jpg");

        char cmdBuf[128];
        if (argc == 2) {
            sprintf(cmdBuf, "feh -x -g %s --reload 0.5 -Z -Y /tmp/cover.jpg &", argv[1]);
        } else {
            printf("Usage: coverdisp [geometry]\n");
            printf("  ex. coverdisp 800x800+50+140\n");
            return 1;
        }

        system(cmdBuf);



        return 0;
}
