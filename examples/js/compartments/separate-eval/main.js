globalThis.x = 0;
globalThis.increment = function() {
	return (1,eval)("x++");
}
globalThis.test = function() {
    trace("app " + increment() + "\n");
}
let compartment = new Compartment();
await compartment.import("mod");
test();
compartment.global.test();
test();
compartment.global.test();
