/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall vdr-fuse.c `pkg-config fuse --cflags --libs` -o vdr-fuse

  Adapted from fusexmp.c
  FUSE filesystem to see the vdr recordings as single mpg files.

  Miguel CV miguelcv  gmail.com 
*/

#define FUSE_USE_VERSION 26


#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>


// PARA QUITAR
#include <syslog.h>
#include <stdlib.h>


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif


char *videopath;


long unsigned
get_size (char *path)
{

  long unsigned tamanototal;
  struct dirent *ent;
  struct stat filestat;
  DIR *dir;
  char *patharchivo;
  int resu;

  patharchivo = malloc (PATH_MAX);

  tamanototal = 0;

  dir = opendir (path);
  if (dir != NULL)
    {
      while ((ent = readdir (dir)) != NULL)
	{
	  if ((strstr (ent->d_name, ".vdr") != NULL) &&	// El archivo es .vdr TODO Añadir soporte .ts
	      (strcmp ("info.vdr", ent->d_name)) &&	// El archivo no es ni info.vdr ni index.vdr
	      (strcmp ("index.vdr", ent->d_name)) &&
	      (strcmp ("resume.vdr", ent->d_name)))
	    {
	      strcpy (patharchivo, path);
	      strcat (patharchivo, "/");
	      strcat (patharchivo, ent->d_name);
	      resu = stat (patharchivo, &filestat);
	      if (resu == 0)
		tamanototal = tamanototal + filestat.st_size;
	    }
	}

    }
  else				// Este else sobra TODO
    {
      tamanototal = 0;
    }
  closedir (dir);
  free (patharchivo);
  return tamanototal;

}

static int
vdr_getattr (const char *path, struct stat *stbuf)
{
  DIR *dp;

  struct stat filestat;

  int res = 0;

  int resu;

  char *pdest;
  char *pathreal;
  char *parte1;


  if ((!(strstr (path, ".rec.mpg\0"))) && (!(strstr (path, "/\0"))))
    return -ENOENT;		//Si no acaba el archivo en .rec.mpg no existe

  pdest = "";
  parte1 = malloc (PATH_MAX);
  memset (parte1, 0, PATH_MAX);
  pathreal = malloc (PATH_MAX);
  memset (pathreal, 0, PATH_MAX);

  memset (stbuf, 0, sizeof (struct stat));
  if (strcmp (path, "/") == 0)
    {
      stbuf->st_mode = S_IFDIR | 0755;
      stbuf->st_nlink = 2;
    }
  else
    {
      pdest = strrchr (path, '_');
      if (pdest == NULL)
	{
	  res = -ENOENT;
	  goto libera1;
	}
      resu = pdest - path;
      if (resu < 0)
	resu = 0;
      strncpy (parte1, path, resu);
      strcpy (pathreal, videopath);
      strcat (pathreal, parte1);
      strcat (pathreal, "/");
      if (pdest)
	strcat (pathreal, pdest + 1);
      pathreal[strlen (pathreal) - 4] = 0;
      // COMPROBAR QUE SEA .REC
      //  syslog(LOG_INFO,"pathreal:%s",pathreal);
      dp = opendir (pathreal);
      if (dp == NULL)
	res = -ENOENT;
      else
	{
	  resu = 0;		// TODO Esto sobra
	  resu = stat (pathreal, &filestat);	//*******************************RESU DEBERIA SER 0 A LA FUERZA 
	  res = 0;
	  stbuf->st_mode = S_IFREG | 0444;
	  stbuf->st_nlink = 1;
	  stbuf->st_size = get_size (pathreal);
	  stbuf->st_mtime = filestat.st_mtime;
	  closedir (dp);
	}
    }
libera1:
  free (parte1);
  free (pathreal);
  return res;
}

static int
vdr_access (const char *path, int mask)
{
//      int res;
//
//      res = access(path, mask);
//      if (res == -1)
//              return -errno;

  return 0;			// Implementar TODO
}

static int
vdr_readlink (const char *path, char *buf, size_t size)
{
//      int res;
//
//      res = readlink(path, buf, size - 1);
//      if (res == -1)
//              return -errno;
//
//      buf[res] = '\0';
  return -1;
}


static int
vdr_readdir (const char *path, void *buf, fuse_fill_dir_t filler,
	     off_t offset, struct fuse_file_info *fi)
{
//      DIR *dp;
  struct dirent **namelist, **namelistsubdir;
  int i, j, k, l;
  char *pathreal, *pathrealsub, *rutafuse, *temporalpath;

  (void) offset;
  (void) fi;

  char *buffer_replace_str;
  char *p;

  buffer_replace_str = malloc (PATH_MAX);

  pathreal = malloc (PATH_MAX);
  memset (pathreal, 0, PATH_MAX);
  pathrealsub = malloc (PATH_MAX);
  memset (pathrealsub, 0, PATH_MAX);
  rutafuse = malloc (PATH_MAX);
  memset (rutafuse, 0, PATH_MAX);
  temporalpath = malloc (PATH_MAX);
  memset (temporalpath, 0, PATH_MAX);

  if (strcmp (path, "/") != 0)
    return -ENOENT;

  filler (buf, ".", NULL, 0);
  filler (buf, "..", NULL, 0);

  strcpy (pathreal, videopath);
  j = scandir (pathreal, &namelist, 0, alphasort);
  if (j < 0)
    syslog (LOG_INFO, "[vdr_readdir] scandir j es:%d", j);
  else
    {
      for (i = 0; i < j; ++i)
	{
	  if (namelist[i]->d_type == DT_DIR &&
	      (strcmp (".", namelist[i]->d_name)) &&
	      (strcmp ("..", namelist[i]->d_name)))
	    {

	      strcpy (pathrealsub, pathreal);
	      strcat (pathrealsub, "/");
	      strcat (pathrealsub, namelist[i]->d_name);
	      l = scandir (pathrealsub, &namelistsubdir, 0, alphasort);
	      if (l < 0)
		syslog (LOG_INFO, "[vdr_readdir] scandir l es:%d", j);
	      else
		{
		  for (k = 0; k < l; ++k)
		    {
		      if (namelistsubdir[k]->d_type == DT_DIR &&
			  (strcmp (".", namelistsubdir[k]->d_name)) &&
			  (strcmp ("..", namelistsubdir[k]->d_name)))
			{
			  if (strstr (namelistsubdir[k]->d_name, ".rec") !=
			      NULL)
			    {

			      if (!(p = strstr (pathrealsub, videopath)))
				strcpy (rutafuse, pathrealsub);	//Is 'orig' even in 'str'?
			      strncpy (buffer_replace_str, pathrealsub, p - pathrealsub);	// Copy characters from 'str' start to 'orig'
			      buffer_replace_str[p - pathrealsub] = '\0';
			      sprintf (buffer_replace_str + (p - pathrealsub),
				       "%s%s", "", p + strlen (videopath));

			      strcpy (temporalpath, buffer_replace_str);

			      if (!(p = strstr (temporalpath, "/")))
				strcpy (rutafuse, temporalpath);	//Is 'orig' even in 'str'?                            
			      strncpy (buffer_replace_str, temporalpath, p - temporalpath);	// Copy characters from 'str' start to 'orig'
			      buffer_replace_str[p - temporalpath] = '\0';
			      sprintf (buffer_replace_str +
				       (p - temporalpath), "%s%s", "",
				       p + strlen ("/"));

			      strcpy (rutafuse, buffer_replace_str);
			      strcat (rutafuse, "_");
			      strcat (rutafuse, namelistsubdir[k]->d_name);
			      strcat (rutafuse, ".mpg");

			      filler (buf, rutafuse, NULL, 0);
			    }
			}
		      free (namelistsubdir[k]);
		    }
		  free (namelistsubdir);
		}
	    }
	  free (namelist[i]);
	}
    }

  free (namelist);
  free (buffer_replace_str);
  free (pathreal);
  free (pathrealsub);
  free (rutafuse);
  free (temporalpath);
  return 0;
}

static int
vdr_mknod (const char *path, mode_t mode, dev_t rdev)
{
  /* On Linux this could just be 'mknod(path, mode, rdev)' but this
     is more portable */

  return -1;
}

static int
vdr_mkdir (const char *path, mode_t mode)
{

  return -1;
}

static int
vdr_unlink (const char *path)
{
  return -1;			// TODO Implementar borrado...
}

static int
vdr_rmdir (const char *path)
{
  return -1;
}

static int
vdr_symlink (const char *from, const char *to)
{
  return -1;
}

static int
vdr_rename (const char *from, const char *to)
{
  return -1;
}

static int
vdr_link (const char *from, const char *to)
{
  return -1;
}

static int
vdr_chmod (const char *path, mode_t mode)
{
  return -1;
}

static int
vdr_chown (const char *path, uid_t uid, gid_t gid)
{
  return -1;
}

static int
vdr_truncate (const char *path, off_t size)
{
  return -1;
}

#ifdef HAVE_UTIMENSAT
static int
vdr_utimens (const char *path, const struct timespec ts[2])
{
  return -1;
}
#endif

static int
vdr_open (const char *path, struct fuse_file_info *fi)
{
  int res;

  int resu;

  char *pdest;
  char *pathreal;
  char *parte1;

  if (!(strstr (path, ".mpg\0")))
    return -ENOENT;

  pdest = "";
  res = -ENOENT;

  parte1 = malloc (PATH_MAX);
  memset (parte1, 0, PATH_MAX);
  pathreal = malloc (PATH_MAX);
  memset (pathreal, 0, PATH_MAX);

  pdest = strrchr (path, '_');
  if (pdest == NULL)
    goto libera2;
  resu = pdest - path;
  if (resu < 0)
    resu = 0;
  strncpy (parte1, path, resu);
  strcpy (pathreal, videopath);
  strcat (pathreal, parte1);
  strcat (pathreal, "/");
  if (pdest)
    strcat (pathreal, pdest + 1);
  pathreal[strlen (pathreal) - 4] = 0;
  if (get_size (pathreal) != 0)
    res = 0;
libera2:

  free (parte1);
  free (pathreal);
  return res;
  //return 0;
}

static int
vdr_read (const char *path, char *buf, size_t size, off_t offset,
	  struct fuse_file_info *fi)
{
  int res;

  (void) fi;
  struct dirent **namelist;
  struct stat filestat;
  //
  int resu, i, j;
  char *pdest;
  char *pathreal;
  char *patharchivo;
  char *parte1;
  long long int tamano, tamanoaleer, leido;
  char *buffertemp;
  //long long int ptr;

  //ptr=0;
  leido = 0;
  tamanoaleer = 0;

  parte1 = malloc (PATH_MAX);
  memset (parte1, 0, PATH_MAX);
  buffertemp = malloc (size);
  memset (buffertemp, 0, size);
  pathreal = malloc (PATH_MAX);
  memset (pathreal, 0, PATH_MAX);
  patharchivo = malloc (PATH_MAX);
  memset (patharchivo, 0, PATH_MAX);

  pdest = strrchr (path, '_');
  resu = pdest - path;
  if (resu < 0)
    resu = 0;
  strncpy (parte1, path, resu);
  strcpy (pathreal, videopath);
  strcat (pathreal, parte1);
  strcat (pathreal, "/");
  if (pdest)
    strcat (pathreal, pdest + 1);
  pathreal[strlen (pathreal) - 4] = 0;

  tamano = get_size (pathreal);

  //syslog(LOG_INFO,"Nos piden offset:%llu,size:%zu,el tamano es %llu",offset,size,tamano);

  if (offset > tamano)
    goto libera;

  // Seguimos sin el else

  j = scandir (pathreal, &namelist, 0, alphasort);
  if (j < 0)
    {
      syslog (LOG_INFO, "[VDR_READ] scandir j es:%d", j);
      leido = -errno;
      goto libera;
    }
  for (i = 0; i < j; i++)
    {
      if (leido == size)
	i = j;
      else
	{
	  if ((strstr (namelist[i]->d_name, ".vdr") != NULL) &&	// El archivo es .vdr
	      (strcmp ("info.vdr", namelist[i]->d_name)) &&	// El archivo no es ni info.vdr ni index.vdr
	      (strcmp ("index.vdr", namelist[i]->d_name)) &&
	      (strcmp ("resume.vdr", namelist[i]->d_name)))
	    {
	      int file = 0;
	      strcpy (patharchivo, pathreal);
	      strcat (patharchivo, "/");
	      strcat (patharchivo, namelist[i]->d_name);
	      file = open (patharchivo, O_RDONLY);
	      if (file == -1)
		{
		  leido = -errno;
		  goto libera;
		}
	      resu = fstat (file, &filestat);
	      if (offset < filestat.st_size)
		{
		  if ((offset + size) < filestat.st_size)
		    {
		      tamanoaleer = size - leido;
		      res = pread (file, buf + leido, tamanoaleer, offset);
		      //        syslog(LOG_INFO,"Al final leemos offset:%llu,size:%zu,leido:%llu,tamanoaleer:%llu,res=%d",offset,size,leido,tamanoaleer,res);   

		    }
		  else
		    {
		      tamanoaleer = filestat.st_size - offset;
		      res = pread (file, buf + leido, tamanoaleer, offset);
		      //        syslog(LOG_INFO,"Al final leemos offset:%llu,size:%zu,leido:%llu,tamanoaleer:%llu,res=%d",offset,size,leido,tamanoaleer,res);     
		      offset = 0;
		    }

		}
	      else
		{
		  offset = offset - filestat.st_size;
		}
	      if (res == -1)
		{
		  res = -errno;
		  goto libera;
		}
	      //     syslog(LOG_INFO,"offset:%llu,size:%zu,el tamano es %llu,archivo:%s",offset,size,tamano,namelist[i]->d_name);
	      //    syslog(LOG_INFO,"hemos leido res:%d",res);
	      leido = leido + res;
	      //     syslog(LOG_INFO,"leido:%llu",leido);
	      close (file);

	    }
	}
    }

libera:
  for (i = 0; i < j; i++)
    free (namelist[i]);
  free (namelist);
  free (parte1);
  free (buffertemp);
  free (pathreal);
  free (patharchivo);
  //syslog(LOG_INFO,"Devuelvo a fuse leido:%llu",leido);
  return leido;
}

static int
vdr_write (const char *path, const char *buf, size_t size,
	   off_t offset, struct fuse_file_info *fi)
{
  return -1;
}

static int
vdr_statfs (const char *path, struct statvfs *stbuf)
{
  int res;

  res = statvfs (videopath, stbuf);
  if (res == -1)
    return -errno;

  return 0;
}

static int
vdr_release (const char *path, struct fuse_file_info *fi)
{
  /* Just a stub.  This method is optional and can safely be left
     unimplemented */

  (void) path;
  (void) fi;
  return 0;
}

static int
vdr_fsync (const char *path, int isdatasync, struct fuse_file_info *fi)
{
  /* Just a stub.  This method is optional and can safely be left
     unimplemented */

  (void) path;
  (void) isdatasync;
  (void) fi;
  return 0;
}

#ifdef HAVE_POSIX_FALLOCATE
static int
vdr_fallocate (const char *path, int mode,
	       off_t offset, off_t length, struct fuse_file_info *fi)
{
  return -1;
}
#endif

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int
vdr_setxattr (const char *path, const char *name, const char *value,
	      size_t size, int flags)
{
  return -1;
}

static int
vdr_getxattr (const char *path, const char *name, char *value, size_t size)
{
  return -1;
}

static int
vdr_listxattr (const char *path, char *list, size_t size)
{
  return -1;
}

static int
vdr_removexattr (const char *path, const char *name)
{
  return -1;
}
#endif /* HAVE_SETXATTR */

static struct fuse_operations vdr_oper = {
  .getattr = vdr_getattr,
  .access = vdr_access,
  .readlink = vdr_readlink,
  .readdir = vdr_readdir,
  .mknod = vdr_mknod,
  .mkdir = vdr_mkdir,
  .symlink = vdr_symlink,
  .unlink = vdr_unlink,
  .rmdir = vdr_rmdir,
  .rename = vdr_rename,
  .link = vdr_link,
  .chmod = vdr_chmod,
  .chown = vdr_chown,
  .truncate = vdr_truncate,
#ifdef HAVE_UTIMENSAT
  .utimens = vdr_utimens,
#endif
  .open = vdr_open,
  .read = vdr_read,
  .write = vdr_write,
  .statfs = vdr_statfs,
  .release = vdr_release,
  .fsync = vdr_fsync,
#ifdef HAVE_POSIX_FALLOCATE
  .fallocate = vdr_fallocate,
#endif
#ifdef HAVE_SETXATTR
  .setxattr = vdr_setxattr,
  .getxattr = vdr_getxattr,
  .listxattr = vdr_listxattr,
  .removexattr = vdr_removexattr,
#endif
};

int
main (int argc, char *argv[])
{
  umask (0);
  struct fuse_args args = FUSE_ARGS_INIT (0, NULL);
  int i;

  openlog ("vdrfs", LOG_PID, LOG_LOCAL0);	//TODO Suprimir esto en real

  for (i = 0; i < argc; i++)
    {
      if (i == 1)
	{
	  //    videopath = malloc (strlen(argv[i]));
	  //    memset(videopath, 0, strlen(argv[i]));
	  videopath = argv[i];
	}
      else
	fuse_opt_add_arg (&args, argv[i]);
    }

  //return fuse_main(argc, argv, &vdr_oper, NULL);
  return fuse_main (args.argc, args.argv, &vdr_oper, NULL);
}
