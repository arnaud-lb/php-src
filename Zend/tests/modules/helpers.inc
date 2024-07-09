<?php

function create_module($file) {
    $contents = file_get_contents($file);
    preg_match_all('#^/\\* FILE: (.*) \\*/$#', $contents, $matches);
    var_dump($matches);
}
