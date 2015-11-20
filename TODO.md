# Todo list

* Look into having a separate thread for callbacks and ditch the async to have
less thread creation and destruction.
    * What is a good way to do this for queuing data packets?
    * Is a seperate thread a good way to go?
* Change so the RX/TX loop is not holding the resource.
