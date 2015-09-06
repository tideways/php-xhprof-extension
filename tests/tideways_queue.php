<?php

namespace Pheanstalk {
    class Pheanstalk {
        protected $_using;

        public function put() {}
        public function putInTube($channel) {
            $this->_using = $channel;
            $this->put();
        }
    }
}

namespace PhpAmqpLib\Channel {
    class AMQPChannel {
        public function basic_publish($msg, $exchange = '', $routing_key = '', $mandatory = false, $immediate = false, $ticket = null) {}
    }
}
