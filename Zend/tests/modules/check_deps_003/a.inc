<?php

module Test;

if (getenv('THIS_IS_DEFINED')) {
    class A implements I {}
}
