#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "http.h"

const int STATUS_TO_CODE[status_t_size] = {
	[OK] = 200,
	[CREATED] = 201,
	[NOT_FOUND] = 404,
};

const char *STATUS_TO_PHRASE[status_t_size] = {
	[OK] = "OK",
	[CREATED] = "Created",
	[NOT_FOUND] = "Not Found",
};

method_t method_valueof(const char *input)
{
	if (strcmp("GET", input) == 0)
		return (GET);

	if (strcmp("POST", input) == 0)
		return (POST);

	return (method_t_size);
}

header_t *headers_add(header_t *previous, const char *key, const char *value)
{
	header_t *header = malloc(sizeof(header_t));

	header->key = strdup(key);
	header->value = strdup(value);
	header->next = previous;

	return (header);
}

header_t *headers_add_number(header_t *previous, const char *key, size_t value)
{
	char buffer[32];
	sprintf(buffer, "%ld", value);

	return (headers_add(previous, key, buffer));
}

void headers_clear(header_t *header)
{
	while (header)
	{
		free(header->key);
		free(header->value);

		header_t *next = header->next;
		free(header);

		header = next;
	}
}

const char *headers_get(header_t *first, const char *key)
{
	header_t *header = first;
	while (header)
	{
		if (strcasecmp(key, header->key) == 0)
			return (header->value);

		header = header->next;
	}

	return ("");
}
