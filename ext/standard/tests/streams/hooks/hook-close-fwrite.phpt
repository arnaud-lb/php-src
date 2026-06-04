--TEST--
Stream hook: closed during write hook
--FILE--
<?php

stream_set_hook(function ($stream) {
    fclose($stream);
    stream_set_hook(null);
});

$fd = tmpfile();
var_dump(fwrite($fd, 'a'));
?>
--EXPECTF--
Warning: fclose(): cannot close the provided stream, as it must not be manually closed in %s on line %d
int(1)
