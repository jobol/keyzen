static int _itoa(int value, char *buffer)
{
	int i, r;
	char c;

	if (!value) {
		buffer[0] = '0';
		r = 1;
	} else {
		if (value < 0) {
			/* handling case of INTEGER_MINIMUM that has no positive value */
			value = -value;
			buffer[0] = '-';
			buffer[1] = '0' + (char)(((unsigned)value) % 10);
			value = (int)(((unsigned)value) / 10);
			i = 2;
		} else {
			i = 0;
		}
		while (value) {
			buffer[i++] = '0' + (char)(value % 10);
			value /= 10;
		}
		r = i;
		while (value < --i) {
			c = buffer[value];
			buffer[value++] = buffer[i];
			buffer[i] = c;
		}
	}
	return r;
}


static void itoa(int value, char *buffer)
{
	buffer[_itoa(value,buffer)] = 0;
}

