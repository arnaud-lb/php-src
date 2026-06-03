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
--EXPECT--

