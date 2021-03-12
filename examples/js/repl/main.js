import REPL from "repl";
import Modules from "modules";

globalThis.console = Object.freeze({
	log: function (...str) {
		REPL.write(...str, newline);
	}
}, true);

globalThis.require = Modules.importNow;

const console = new REPL;
