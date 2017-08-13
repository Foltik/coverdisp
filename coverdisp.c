#include <mpd/client.h>
#include <mpd/status.h>
#include <mpd/tag.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#define RED "\x1b[31;01m"
#define GREEN "\x1b[32;06m"
#define RESET "\x1b[0m"

#define MPD_ROOT_DIRECTORY "/home/lain/Music/beets/"

void copy(const char* source, const char* dest) {
	int inFile = open(source, O_RDONLY);
	int outFile = open(dest, O_WRONLY);
	
	if (inFile < 0) {
		printf("\x1b[31;01m""Error: ""\x1b[0m""Could not open input file \"%s\"\n", source);
		return;
	}
	if (outFile < 0) {
		printf("\x1b[31;01m""Error: ""\x1b[0m""Could not open output file \"%s\"\n", dest);
		return;
	}

	char buf[8192];
	while (true) {
		ssize_t result = read(inFile, &buf[0], sizeof(buf));
		if (!result) break;
		assert(result > 0);
		assert(write(outFile, &buf[0], result) == result);
	}

	close(inFile);
	close(outFile);
}

int main(int argc, char** argv) {
	struct mpd_connection* conn;
	conn = mpd_connection_new(NULL, 0, 30000);

	if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) {
		printf(RED"Error: "RESET"Could not establish connection with MPD\n");
		return 1;
	}

	char cmdBuf[128];
	if (argc == 2) {
		sprintf(cmdBuf, "feh -x -g %s --reload 0.5 -Z -Y /tmp/cover.jpg &", argv[1]);
	} else {
		printf("Usage: coverdisp [geometry]\n");
		printf("  ex. coverdisp 800x800+50+140\n");
		return 1;
	}

	system(cmdBuf);

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

	return 0;
}
