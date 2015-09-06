<?php

namespace Doctrine\ORM {
    class EntityManager {
        public function flush() {
        }
    }
    abstract class AbstractQuery {
        protected $_resultSetMapping;
        public function setResultSetMapping($rsm) {
            $this->_resultSetMapping = $rsm;
        }
        public function execute()
        {
        }
    }
    class Query extends AbstractQuery {
        public function getDQL() {
            return 'SELECT f FROM Foo\\Bar\\Baz';
        }
    }
    class NativeQuery extends AbstractQuery {
        public function getSQL() {
            return 'SELECT foo FROM bar';
        }
    }
}
namespace Doctrine\ORM\Query {
    class ResultSetMapping {
        public $aliasMap = array();
    }
}

namespace Doctrine\ORM\Persisters {
    use Doctrine\ORM\Mapping\ClassMetadata;

    class BasicEntityPersister {
        protected $class;

        public function __construct($name) {
            $this->class = new ClassMetadata();
            $this->class->name = $name;
        }

        public function load() {
        }
    }
}

namespace Doctrine\ORM\Mapping {
    class ClassMetadata {
        public $name;
    }
}

namespace Doctrine\CouchDB\HTTP {
    abstract class AbstractHTTPClient {
        private $options;

        public function __construct($host = 'localhost') {
            $this->options['host'] = $host;
        }
    }
    class StreamClient extends AbstractHTTPClient {
        public function request($method, $path) {
            var_dump($method ." " . $path);
        }
    }
    class SocketClient extends AbstractHTTPClient {
        public function request($method, $path) {
            var_dump($method ." " . $path);
        }
    }
}
