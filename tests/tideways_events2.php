<?php

class Mage
{
    static public function dispatchEvent($eventName, $params)
    {
    }
}

class Enlight_Event_EventManager
{
    public function filter($event, $value, $args) {}
    public function notify($event, $args) {}
    public function notifyUntil($event, $args) {}
}

function do_action($name, $params) {
}
function apply_filters($name, $params) {
}
function drupal_alter($name, $args) {
}
