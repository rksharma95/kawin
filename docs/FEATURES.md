# kawin features

## Events

### Process
- [ ] **execution**
- [ ] fileless execution
- [ ] integrity monitoring using hash
- [ ] external execution and ssh
### File
- [ ] **open**
- [ ] read
- [ ] rename
- [ ] write
- [ ] integrity monitoring using hash
- [ ] permissions (security descriptor in windows)
    - [ ] change [chown]
    - [ ] escalation [ch(uid/gid)]
    - [ ] tracability [ptrace]
- [ ] directories
    - [ ] create
    - [ ] access
    - [ ] rename
    - [ ] delete
- [ ] filesystem/volume
    - mount
    - unmount
    - protect critical fs i.e. `/proc` (if any with windows)
### Network
- [ ] process based trasport-layer events
    - [ ] socket
    - [ ] bind
    - [ ] connect
    - [ ] listen
    - [ ] accept
- [ ] DNS monitoring
- [ ] network bandwidth monitoring
### Device
- [ ] types
    - [ ] USB
        - [ ] connection

## Workloads

- [ ] **vm/bare-metal**
- [ ] containers
    - [ ] orchestrated
    - [ ] unorchestrated

## Security Policies

- [ ] Process Rules
    - [ ] **path**: enforce rule on a process executable using full path 
    - [ ] directory: enforce rule on all the process executables in a directory
    - [ ] execname: enforce rule using executable name
    - [ ] fromSource: enforce rule on a process based on parent process

- [ ] File Rules:
    - [ ] **path**: enforce rule on a file using full path 
    - [ ] directory: enforce rule on all the files in a directory
    - [ ] fromSource: enforce rule on a file based on the process that accessed the file.

- [ ] Network Rules:
    - [ ] protocol: enforce rule based on network protocol.
    - [ ] fromSource: enforce process based network protocol rule.

- [ ] Device Rules:
    - [ ] class: enforce rule based on device class, i.e. MASS-STORAGE.
    - [ ] subClass: enforce rule based on device class+subclass level.
    - [ ] protocol: enforce rule based on device class+subclass+protocol level.
