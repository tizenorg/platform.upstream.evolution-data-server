/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#include <stdio.h>
#include <libebook-contacts/libebook-contacts.h>

gint
main (gint argc,
      gchar **argv)
{
	FILE *fp;
	EVCard *vcard;
	GString *str = g_string_new ("");
	gchar *parsed_vcard;

	if (argc < 2) {
		g_warning ("Requires one parameter, a vCard file\n");
		return 1;
	}

	fp = fopen (argv[1], "r");
	if (fp == NULL) {
		g_warning ("Faile to open vCard file '%s'", argv[1]);
		return 1;
	}

	while (!feof (fp)) {
		gchar buf[1024];
		if (fgets (buf, sizeof (buf), fp))
			str = g_string_append (str, buf);
	}
	fclose (fp);

	vcard = e_vcard_new_from_string (str->str);
	g_string_free (str, TRUE);

	e_vcard_dump_structure (vcard);

	parsed_vcard = e_vcard_to_string (vcard, EVC_FORMAT_VCARD_21);
	printf ("\nvCard 2.1: %s\n", parsed_vcard);
	g_free (parsed_vcard);

	parsed_vcard = e_vcard_to_string (vcard, EVC_FORMAT_VCARD_30);
	printf ("\nvCard 3.0: %s\n", parsed_vcard);
	g_free (parsed_vcard);

	g_object_unref (vcard);

	return 0;
}
