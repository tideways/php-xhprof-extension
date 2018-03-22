<?php

namespace Elasticsearch\Connections {

    class Connection {
        public function performRequest($method, $uri, $params = null, $body = null, $options = array(), $transport = null)
        {
        }
    }

    class GuzzleConnection {
        public function performRequest($method, $uri, $params = null, $body = null, $options = array()) {
        }
    }
}

namespace Elasticsearch\Endpoints {
    abstract class AbstractEndpoint {
        public function resultOrFuture($result) {
        }
    }

    class SomeEndpoint extends AbstractEndpoint {
    }

    class AnotherEndpoint extends AbstractEndpoint {
    }
}
