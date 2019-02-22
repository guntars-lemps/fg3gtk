#ifndef SERIAL_H
#define SERIAL_H

/*
// returns list of serial devices
char** get_serial_devices()
{
    printf("get_serial_devices()\n");

    // cat /proc/tty/drivers

    FILE * str;
    char buffer[30];

    f_drivers = fopen("/proc/tty/drivers", "r");

    if (f_drivers == NULL) {
        printf("Cannot open \"/proc/tty/drivers\"\n");
        return NULL
    }

    while (EOF != fscanf(f_drivers, "%[^\n]\n", str))
    {
        printf("> %s\n", str);
    }

    fclose(f_drivers);
    return (0);

    // pēc tam tie kuriem beidzas ar serial

    //

    // Couldn't find it, try all the tty drivers.
        if (i == 3) {
          FILE *fp = fopen("/proc/tty/drivers", "r");
          int tty_major = 0, maj = dev_major(rdev), min = dev_minor(rdev);
          if (fp) {
            while (fscanf(fp, "%*s %256s %d %*s %*s", buf, &tty_major) == 2) {
              // TODO: we could parse the minor range too.
              if (tty_major == maj) {
                len = strlen(buf);
                len += sprintf(buf+len, "%d", min);
                if (!stat(buf, &st) && S_ISCHR(st.st_mode) && st.st_rdev==rdev)
                  break;
              }
              tty_major = 0;
            }
            fclose(fp);
          }
          // Really couldn't find it, so just show major:minor.
          if (!tty_major) len = sprintf(buf, "%d:%d", maj, min);
        }

}
*/





#endif
