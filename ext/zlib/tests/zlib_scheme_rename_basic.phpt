--TEST--
Test compress.zlib:// scheme with the unlink function
--EXTENSIONS--
zlib
--FILE--
<?php
$inputFileName = __DIR__."/data/test.txt.gz";
$srcFile = "compress.zlib://$inputFileName";
rename($srcFile, 'something.tmp');
var_dump(file_exists($inputFileName));
?>
--EXPECTF--
Warning: rename(): ZLIB wrapper does not support renaming in %s on line %d
bool(true)
