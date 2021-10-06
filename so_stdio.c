#include "so_stdio.h"
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

FUNC_DECL_PREFIX SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	SO_FILE *stream = malloc(sizeof(struct _so_file));
	/* initializare campuri */
	stream->buff_offset = -1;
	stream->wbuff_contor = 0;
	stream->last_op = -1;
	stream->cursor_pos = 0;
	stream->feof_check = 0;
	stream->ferror_check = 0;
	stream->bytes_read = BUFFER_SIZE;

	if (!strcmp(mode, "r"))
		stream->fd = open(pathname, O_RDONLY, 0644);
	else if (!strcmp(mode, "w"))
		stream->fd = open(pathname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	else if (!strcmp(mode, "a"))
		stream->fd = open(pathname, O_WRONLY | O_CREAT | O_APPEND, 0644);
	else if (!strcmp(mode, "r+"))
		stream->fd = open(pathname, O_RDWR, 0644);
	else if (!strcmp(mode, "w+"))
		stream->fd = open(pathname, O_RDWR | O_CREAT | O_TRUNC, 0644);
	else if (!strcmp(mode, "a+"))
		stream->fd = open(pathname, O_RDWR | O_CREAT | O_APPEND, 0644);
	else
		stream->fd = -1;

	/* open returneaza -1 daca fisierul nu exista si noul fd in caz contrar*/
	if (stream->fd <= -1) {
		free(stream);
		return NULL;
	}
	return stream;
}

FUNC_DECL_PREFIX int so_fclose(SO_FILE *stream)
{
	int rc;
	/* Daca ultima operatie a fost de scriere, se scrie in fisier */
	if (stream->last_op == WRITE) {
		rc = write(stream->fd, &stream->write_buff, stream->wbuff_contor);
		if (rc < 0) {
			return SO_EOF;
			stream->ferror_check = 1;
		}
		stream->cursor_pos += rc;
	}

	rc = close(stream->fd);
	if (rc == -1)
		stream->ferror_check = 1;
	free(stream);
	return rc;
}

FUNC_DECL_PREFIX int so_fileno(SO_FILE *stream)
{
	/* returnez file descriptorul structurii */
	return stream->fd;
}

FUNC_DECL_PREFIX int so_fflush(SO_FILE *stream)
{
	int rc = 0;
	/* daca ultima operatie a fost una de citire, se scrie continutul bufferului */
	/* in fisier si se elibereaza. Verific codul de eroare intors de write */
	if (stream->last_op == WRITE) {
		rc = write(stream->fd, &stream->write_buff, stream->wbuff_contor);
		if (rc < 0) {
			return SO_EOF;
			stream->ferror_check = 1;
		}
	}
	stream->cursor_pos += rc;
	strcpy(stream->read_buff, "");
	return 0;
}

FUNC_DECL_PREFIX int so_fseek(SO_FILE *stream, long offset, int whence)
{
	off_t rc;
	/* daca ultima operatie este de citire eliberez bufferul */
	if (stream->last_op == READ) {
		strcpy(stream->read_buff, "");
		stream->buff_offset = -1;
	}
	/* daca ultima operatie este de scriere, scriu in fisier continutul bufferului */
	if (stream->last_op == WRITE) {
		rc = write(stream->fd, &stream->write_buff, stream->wbuff_contor);
		stream->cursor_pos += rc;
	}
	rc = lseek(stream->fd, offset, whence);
	if (rc == -1) {
		stream->ferror_check = 1;
		return -1;
	}
	stream->cursor_pos = rc;
	return 0;

}

FUNC_DECL_PREFIX long so_ftell(SO_FILE *stream)
{
	/* returneaza pozitia cursorului in fisier */
	if (stream->cursor_pos < 0)
		return -1;
	return stream->cursor_pos;
}

FUNC_DECL_PREFIX size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	int total_nmemb = size * nmemb;
	int rc = 0;
	int i = 0;
	char *p = ptr;

	p = ptr;
	/* Loop pentru catu bytes sunt de citit */
	/* Folosesc so_fgetc pentru a citi si apoi pentru a adauga la adresa */
	/* de memorie corespunzatoare */
	while (i < total_nmemb) {
		rc = so_fgetc(stream);
		p[i] = rc;
		i++;
		stream->cursor_pos++;
	}
	if (stream->ferror_check == 1 || stream->feof_check == 1)
		return 0;
	return nmemb;

}

FUNC_DECL_PREFIX size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	int total_nmemb = size * nmemb;
	int i;
	char *p = (char *)ptr;

	i = 0;
	while (i < total_nmemb) {
		so_fputc((unsigned char)p[i], stream);
		i++;
		stream->cursor_pos++;
	}
	if (stream->ferror_check == 1)
		return 0;
	return nmemb;
}

FUNC_DECL_PREFIX int so_fgetc(SO_FILE *stream)
{
	int rc;
	/* verific daca bufferul e plin sau daca e gol si citesc in continuare */
	/* din el. Pentru cazul cand sunt mai putini bytes cititi modific */
	/* bytes_read din stream pentru a tine cont de asta */
	stream->last_op = READ;
	if (stream->buff_offset >= BUFFER_SIZE - 1 || stream->buff_offset == -1) {
		strcpy(stream->read_buff, "");
		rc = read(stream->fd, &stream->read_buff, BUFFER_SIZE);
		stream->buff_offset = -1;
		if (rc == -1) {
			stream->ferror_check = 1;
			return SO_EOF;
		}
		if (rc == 0) {
			stream->feof_check = 1;
			return SO_EOF;
		}
		if (rc < BUFFER_SIZE)
			stream->bytes_read = rc;
	}
	/* Caz de verificare eof pentru cand bytes cititi sunt mai putini */
	/* decat BUFFER_SIZE */
	stream->buff_offset++;
	if (stream->bytes_read < BUFFER_SIZE &&
		stream->buff_offset == stream->bytes_read) {
		stream->feof_check = 1;
		return SO_EOF;
	}
	return (unsigned int)stream->read_buff[stream->buff_offset];
}

FUNC_DECL_PREFIX int so_fputc(int c, SO_FILE *stream)
{
	int rc;
	/* verific daca am ajuns la capacitatea maxima */
	/* a bufferului si scriu in fisier. Returnez SO_EOF */
	/* daca a exista o eroare */
	stream->last_op = WRITE;
	if (stream->wbuff_contor >= BUFFER_SIZE) {
		rc = write(stream->fd, &stream->write_buff, BUFFER_SIZE);
		stream->wbuff_contor = 0;
		memset(stream->write_buff, 0, BUFFER_SIZE);
		if (rc == -1) {
			stream->ferror_check = 1;
			return SO_EOF;
		}
	}
	stream->write_buff[stream->wbuff_contor] = (char)c;
	stream->wbuff_contor++;
	return c;
}

FUNC_DECL_PREFIX int so_feof(SO_FILE *stream)
{
	/* verific daca falg-ul de feof este activat */
	if (stream->feof_check == 1)
		return SO_EOF;
	return 0;
}

FUNC_DECL_PREFIX int so_ferror(SO_FILE *stream)
{
	/* verific daca flag-ul de eroare este activat*/
	if (stream->ferror_check == 1)
		return SO_EOF;
	return 0;
}

FUNC_DECL_PREFIX SO_FILE *so_popen(const char *command, const char *type)
{
	return NULL;
}

FUNC_DECL_PREFIX int so_pclose(SO_FILE *stream)
{
	return 0;
}
