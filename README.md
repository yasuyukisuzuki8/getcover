# getcover

** getcover - get cover art image from music files **

 Supported Formats are:  
  FLAC: Free Lossless Audio Codec  
  ALAC: Apple Lossless Audio Codec (m4a)  
  AAC: Advanced Audio Coding (m4a)  

** Compile: **
```
 $ make  
 $ sudo make install
```

** Usage: **
```
 getcover [-v] [-o] [-f basename] path [path [path]...]
```
 *  -v: verbose mode  
 *  -o: override mode. override Folder.jpg even if it exists  
 *  -f basename: specifiy name of image file without suffix  
 *  path: path of the directory which contains music files  

 
** Example: **

To extract a cover art from music files in the current directory:  
```
 $ getcover . 
```

Specifiy filename for cover art. Cover.jpg for JPEG data or Cover.png for PNG data.  
```
 $ getcover -f Cover .
```

To extract cover arts from music files in all sub directries of the specified directory:  
```
 $ getcover /var/lib/mpd/music/*
```

To extract cover arts from music files in all sub directories under current directory:  
```
 $ find . -type d | xargs getcover
```

