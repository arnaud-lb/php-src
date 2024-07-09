<?php

module Foo;
namespace Bar;

printf("%s\n", __MODULE__);
printf("%s\n", __NAMESPACE__);

class C {}
printf("%s\n", get_class(new C()));
printf("%s\n", new \ReflectionClass(new C())->getModuleName());

