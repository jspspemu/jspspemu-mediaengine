type int = number;
export interface pointer { __pointer:string; }

const enum AudioFormat {
	MP2     = 0x15000,
	MP3     = 0x15001,
	ATRAC3  = 0x1501F,
	ATRAC3P = 0x15028,
}

export enum ME_MediaType {
	Unknown = -1,
	Video = 0,
	Audio = 1,
	Data = 2,
	Subtitle = 3,
	Attachment = 4,
	Nb = 5,
}

declare var FS:any;
declare var HEAP8:Uint8Array;
declare function _malloc(size:int):pointer;
declare function _free(ptr:pointer):void;
declare function writeStringToMemory(str:string, ptr:pointer):void;

declare function _av_malloc(size:int):pointer;
declare function _av_free(ptr:pointer):void;


export interface ME_DecodeState { __ME_DecodeState:number; }
export interface ME_Packet { __ME_Packet:number; }
export interface ME_BufferData { __ME_BufferData:number; }

declare class Runtime {
	static addFunction(callback: Function):number;
}

// STREAM
declare function _me_init():void;
declare function _me_open(name:pointer, filearg:number, readfunc:number, writefunc:number, seekfunc:number):ME_DecodeState;
declare function _me_close(state:ME_DecodeState):void;
declare function _me_seek(state:ME_DecodeState, timestamp:number):void;

// PACKET
declare function _me_packet_read(state:ME_DecodeState):ME_Packet;
declare function _me_packet_free(packet:ME_Packet):void;
declare function _me_packet_get_type(packet:ME_Packet):ME_MediaType;
declare function _me_packet_get_pts(packet:ME_Packet):number;
declare function _me_packet_get_dts(packet:ME_Packet):number;
declare function _me_packet_get_pos(packet:ME_Packet):number;
declare function _me_packet_get_duration(packet:ME_Packet):number;

// BUFFER
declare function _me_buffer_get_size(buffer:ME_BufferData):number;
declare function _me_buffer_get_data(buffer:ME_BufferData):pointer;
declare function _me_buffer_alloc(size:number):ME_BufferData;
declare function _me_buffer_alloc_copy_data(data:pointer, size:number):ME_BufferData;
declare function _me_buffer_free(buffer:ME_BufferData):void;

// AUDIO
declare function _me_packet_decode_audio(state:ME_DecodeState, packet:ME_Packet, channels:number, rate:number):ME_BufferData;

_me_init();

export class MeBuffer {
	constructor(public buffer:ME_BufferData) { }
	static alloc(size:number) { return new MeBuffer(_me_buffer_alloc(size)); }
	get size():number { return _me_buffer_get_size(this.buffer); }
	get pointer():pointer { return _me_buffer_get_data(this.buffer); }
	get data():Uint8Array { return HEAP8.subarray(<any>this.pointer, <any>this.pointer + this.size); }
	get datas16():Int16Array {
		var data = this.data;
		return new Int16Array(data.buffer, data.byteOffset, data.byteLength / 2);
	}
	free() { _me_buffer_free(this.buffer); }
}

export class MeString {
	public ptr:pointer;

	constructor(public name:string) {
		this.ptr = _malloc(name.length + 1);
		writeStringToMemory(name, this.ptr);
	}
	free() {
		_free(this.ptr);
	}
}

export class MePacket {
	constructor(public stream:MeStream, public packet:ME_Packet) {
	}
	get type():ME_MediaType { return _me_packet_get_type(this.packet); }
	get pts():number { return _me_packet_get_pts(this.packet); }
	get dts():number { return _me_packet_get_dts(this.packet); }
	get pos():number { return _me_packet_get_pos(this.packet); }
	get duration():number { return _me_packet_get_duration(this.packet); }

	decodeAudioFramesAndFree(channels:number, rate:number):Int16Array[] {
		var frames:Int16Array[] = [];
		for (var n = 0; n < 10000; n++) {
			var frame = this.decodeAudio(channels, rate);
			if (frame == null) break;
			frames.push(frame);
		}
		this.free();
		return frames;
	}
	decodeAudio(channels:number, rate:number):Int16Array {
		var buffer_ptr = _me_packet_decode_audio(this.stream.state, this.packet, channels, rate);
		if (<any>buffer_ptr == 0) return null;
		var buffer = new MeBuffer(buffer_ptr);
		var out = new Int16Array(buffer.size / 2);
		out.set(buffer.datas16);
		buffer.free();
		return out;
	}
	decodeAudioAndFree(channels:number, rate:number):Int16Array {
		try {
			return this.decodeAudio(channels, rate);
		} finally {
			this.free();
		}
	}
	free() { _me_packet_free(this.packet); }
}

export enum SeekType {
	Set = 0,
	Cur = 1,
	End = 2,
	Tell = 0x10000,
}

export class CustomStream {
	static items:{[key:number]:CustomStream} = {};
	static lastId = 0;
	static alloc(stream:CustomStream) {
		var id = this.lastId++;
		this.items[id] = stream;
		return id;
	}
	static free(id:number) {
		delete this.items[id];
	}
	constructor(public name:string = 'customstream') { }
	
	public read(buf:Uint8Array):number {
		throw new Error("Must override CustomStream.read()");
	}
	public write(buf:Uint8Array):number {
		throw new Error("Must override CustomStream.write()");
	}
	private _position: number = 0;
	get length():number {
		throw new Error("Must override CustomStream.seek()");
	}
	get available():number {
		return this.length - this.position;
	}
	get position() {
		return this._position;
	}
	set position(value:number) {
		this._position = value;
	}
	
	public _read(buf:Uint8Array):number {
		var result = this.read(buf);
		if (result >= 0) this.position += result;
		return result;
	}

	public _write(buf:Uint8Array):number {
		var result = this.write(buf);
		if (result >= 0) this.position += result;
		return result;
	}

	public _seek(offset: number, whence: SeekType) {
		switch (whence) {
			case SeekType.Set: this.position = 0 + offset; break;
			case SeekType.Cur: this.position = this.position + offset; break;
			case SeekType.End: this.position = this.length + offset; break;
			case SeekType.Tell: return this.position;
		}
		return this.position;
	}
	public close():void {
	}
}

var readfunc = Runtime.addFunction((opaque:number, buf:number, buf_size:number) => {
	//console.log('readfunc', opaque, buf, buf_size);
	return CustomStream.items[opaque]._read(HEAP8.subarray(buf, buf + buf_size));
});
var writefunc = Runtime.addFunction((opaque:number, buf:number, buf_size:number) => {
	//console.log('writefunc', opaque, buf, buf_size);
	return CustomStream.items[opaque]._write(HEAP8.subarray(buf, buf + buf_size));
});
var seekfunc = Runtime.addFunction(function(opaque:number, offsetLow:number, offsetHight:number, whence:SeekType) {
	//console.log('seekfunc', opaque, offset, whence);
	return CustomStream.items[opaque]._seek(offsetLow + offsetHight * Math.pow(2, 32), whence);
});

export class MemoryCustomStream extends CustomStream {
	constructor(public data:Uint8Array) {
		super();
	}
	get length() { return this.data.length; }
	
	public read(buf:Uint8Array):number {
		//console.log('read', buf.length);
		var readlen = Math.min(buf.length, this.available);
		if (readlen > 0) {
			buf.subarray(0, readlen).set(this.data.subarray(this.position, this.position + readlen));
		}
		return readlen;
	}
	public write(buf:Uint8Array):number {
		throw new Error("Must override CustomStream.write()");
	}
	public close():void {
	}
}

export class MeStream {
	constructor(public customStream:CustomStream, public state:ME_DecodeState, public onclose?:(stream:MeStream) => void) {
	}
	close() {
		if (this.onclose) this.onclose(this);
		_me_close(this.state);
	}
	readPacket() {
		var v = _me_packet_read(this.state);
		if ((<any>v) == 0) return null;
		return new MePacket(this, v);
	}
	seek(timestamp:number) {
		_me_seek(this.state, timestamp);
	}
	get position() { return this.customStream.position; }
	get length() { return this.customStream.length; }
	static open(customStream:CustomStream):MeStream {
		var nameStr = new MeString(customStream.name);
		try {
			var csid = CustomStream.alloc(customStream);
			return new MeStream(customStream,
				_me_open(nameStr.ptr, csid, readfunc, writefunc, seekfunc), () => {
					customStream.close();
					CustomStream.free(csid);
				});
		} finally {
			nameStr.free();
		}
	}
	static openData(data:Uint8Array) {
		return this.open(new MemoryCustomStream(data));
	}
}

/*
var compressedData = new Uint8Array([
    0xFF, 0xF3, 0x14, 0xC4, 0x00, 0x02, 0x70, 0x3A, 0xEC, 0x01, 0x43, 0x00, 0x01, 0x77, 0x77, 0x77,
    0x38, 0x80, 0x60, 0x60, 0x60, 0x6E, 0x3D, 0x87, 0xFF, 0xF3, 0x14, 0xC4, 0x01, 0x02, 0x80, 0x3B,
    0x04, 0x01, 0x8B, 0x00, 0x00, 0x7F, 0x90, 0x40, 0xC2, 0xA1, 0x24, 0x81, 0x93, 0xCE, 0x00, 0x0A,
    0xFF, 0xF3, 0x14, 0xC4, 0x02, 0x01, 0xB8, 0x37, 0x21, 0xE1, 0xC2, 0x00, 0x03, 0x3F, 0xF1, 0x4C,
    0x04, 0x14, 0x00, 0x0C, 0x2B, 0x80, 0x2C, 0x20, 0xFF, 0xF3, 0x14, 0xC4, 0x06, 0x01, 0xB0, 0x37,
    0x0D, 0xE0, 0x01, 0xCC, 0x27, 0x32, 0x01, 0x0E, 0x2B, 0x09, 0x44, 0xE9, 0x00, 0x0C, 0x20, 0xCC,
    0xFF, 0xF3, 0x14, 0xC4, 0x0A, 0x01, 0x80, 0x37, 0x21, 0xE0, 0x00, 0x4A, 0x23, 0x55, 0x55, 0x00,
    0x0C, 0x21, 0x32, 0x0A, 0x12, 0x00, 0x0C, 0x20, 0xFF, 0xF3, 0x14, 0xC4, 0x0F, 0x01, 0x58, 0x37,
    0x0D, 0xE0, 0x02, 0x8C, 0x43, 0xBE, 0x14, 0x0D, 0x00, 0x0C, 0x20, 0x48, 0xD1, 0x8A, 0x90, 0x8D,
    0xFF, 0xF3, 0x14, 0xC4, 0x14, 0x01, 0x78, 0x37, 0x0D, 0xE0, 0x02, 0x8C, 0x43, 0x56, 0x74, 0x4C,
    0xDB, 0x51, 0xBF, 0xCA, 0xBA, 0xC2, 0xF5, 0x4E, 0xFF, 0xF3, 0x14, 0xC4, 0x19, 0x01, 0x78, 0x37,
    0x15, 0xE0, 0x01, 0xCC, 0x43, 0x72, 0x0C, 0xB5, 0x31, 0x9E, 0x00, 0x1E, 0x20, 0x3A, 0x3C, 0x4D,
    0xFF, 0xF3, 0x14, 0xC4, 0x1E, 0x01, 0x58, 0x37, 0x19, 0xE0, 0x01, 0x4C, 0x43, 0x00, 0x0C, 0x21,
    0x26, 0x08, 0x59, 0x00, 0x0C, 0x21, 0x94, 0x2E, 0xFF, 0xF3, 0x14, 0xC4, 0x23, 0x02, 0x20, 0x3A,
    0xC4, 0x00, 0x03, 0xF4, 0x20, 0x95, 0x00, 0x0E, 0x21, 0x38, 0x11, 0x25, 0x00, 0x0C, 0x20, 0x22,
    0xFF, 0xF3, 0x14, 0xC4, 0x25, 0x02, 0x30, 0x3A, 0xC4, 0x00, 0x06, 0x84, 0x24, 0x12, 0x12, 0x00,
    0x1E, 0x20, 0x16, 0xC1, 0x11, 0xAD, 0x10, 0x0D, 0xFF, 0xF3, 0x14, 0xC4, 0x27, 0x01, 0x68, 0x37,
    0x21, 0xE0, 0x01, 0xCC, 0x43, 0x75, 0x57, 0x84, 0xA3, 0x84, 0x2A, 0x9D, 0xF5, 0x0C, 0x08, 0x35,
    0xFF, 0xF3, 0x14, 0xC4, 0x2C, 0x01, 0x70, 0x37, 0x09, 0xE0, 0x02, 0x8C, 0x43, 0x82, 0xE8, 0x15,
    0xD3, 0x34, 0xCD, 0xB5, 0x00, 0x1E, 0x20, 0x12, 0xFF, 0xF3, 0x14, 0xC4, 0x31, 0x01, 0x60, 0x37,
    0x0D, 0xE0, 0x02, 0x8A, 0x43, 0xE2, 0x46, 0xF1, 0x00, 0xCF, 0xE8, 0x87, 0x81, 0x31, 0x84, 0x8A,
    0xFF, 0xF3, 0x14, 0xC4, 0x36, 0x01, 0x60, 0x37, 0x21, 0xE0, 0x01, 0x44, 0x43, 0xF3, 0xC5, 0x61,
    0xDC, 0x04, 0x31, 0xDA, 0x08, 0x61, 0xA8, 0x18, 0xFF, 0xF3, 0x14, 0xC4, 0x3B, 0x01, 0x78, 0x37,
    0x11, 0xE0, 0x01, 0xCC, 0x43, 0xD7, 0xC6, 0x21, 0x5F, 0x49, 0x9A, 0x6A, 0xAF, 0x32, 0xC4, 0x80,
    0xFF, 0xF3, 0x14, 0xC4, 0x40, 0x01, 0xB8, 0x3F, 0x19, 0xE0, 0x01, 0xD0, 0x42, 0x48, 0xC2, 0x3B,
    0xA0, 0xBD, 0x15, 0x4C, 0x41, 0x4D, 0x45, 0x33, 0xFF, 0xF3, 0x14, 0xC4, 0x44, 0x02, 0x60, 0x42,
    0xFD, 0xE0, 0x03, 0x58, 0x0A, 0x2E, 0x39, 0x39, 0x2E, 0x33, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0xFF, 0xF3, 0x14, 0xC4, 0x45, 0x02, 0x60, 0x3E, 0xED, 0xE0, 0x00, 0x9E, 0x06, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xFF, 0xF3, 0x14, 0xC4, 0x46, 0x01, 0xB0, 0x3F,
    0x19, 0xE0, 0x01, 0xD2, 0x42, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0xFF, 0xF3, 0x14, 0xC4, 0x4A, 0x01, 0xD8, 0x37, 0x09, 0x41, 0x4A, 0x00, 0x00, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0xFF, 0xF3, 0x14, 0xC4, 0x4D, 0x04, 0x20, 0x4E,
    0xEC, 0x01, 0x90, 0x00, 0x00, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55,
    0xFF, 0xF3, 0x14, 0xC4, 0x47, 0x02, 0xB8, 0x2A, 0xB8, 0x01, 0xC6, 0x00, 0x01, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55 
]);

var stream = MeStream.openData(compressedData);
while (true) {
	var packet = stream.readPacket();
	if (packet == null) break;
	//console.log(packet.decodeAudioAndFree(2, 44100));
	//break;
	console.log(packet.decodeAudioAndFree(2, 44100));
}
stream.close();
*/