#include "ncbind/ncbind.hpp"
#include <vector>
using namespace std;
#include <process.h>

#define WM_SAVE_TLG_PROGRESS (WM_APP+4)
#define WM_SAVE_TLG_DONE     (WM_APP+5)

// Layer �N���X�����o
static iTJSDispatch2 *layerClass = NULL;         // Layer �̃N���X�I�u�W�F�N�g
static iTJSDispatch2 *layerAssignImages = NULL;  // Layer.setTime ���\�b�h


#include <tlg5/slide.h>
#define BLOCK_HEIGHT 4
//---------------------------------------------------------------------------

typedef bool ProgressFunc(int percent, void *userdata);

// ���k�����p

class Compress {

protected:
	vector<unsigned char> data; //< �i�[�f�[�^
	ULONG cur;                  //< �i�[�ʒu
	ULONG size;                 //< �i�[�T�C�Y
	ULONG dataSize;             //< �f�[�^�̈�m�ۃT�C�Y

public:
	/**
	 * �R���X�g���N�^
	 */
	Compress() {
		cur = 0;
		size = 0;
		dataSize = 1024 * 100;
		data.resize(dataSize);
	}

	/**
	 * �T�C�Y�ύX
	 * �w��ʒu���͂��邾���̃T�C�Y���m�ۂ���B
	 * �w�肵���ő�T�C�Y��ێ�����B
	 * @param �T�C�Y
	 */
	void resize(size_t s) {
		if (s > size) {
			size = s;
			if (size > dataSize) {
				dataSize = size * 2;
				data.resize(dataSize);
			}
		}
	}

	/**
	 * 8bit���l�̏����o��
	 * @param num ���l
	 */
	void writeInt8(int num) {
		resize(cur + 1);
		data[cur++] = num & 0xff;
	}
	
	/**
	 * 32bit���l�̏����o��
	 * @param num ���l
	 */
	void writeInt32(int num) {
		resize(cur + 4);
		data[cur++] = num & 0xff;
		data[cur++] = (num >> 8) & 0xff;
		data[cur++] = (num >> 16) & 0xff;
		data[cur++] = (num >> 24) & 0xff;
	}

	/**
	 * 32bit���l�̏����o��
	 * @param num ���l
	 */
	void writeInt32(int num, int cur) {
		resize(cur + 4);
		data[cur++] = num & 0xff;
		data[cur++] = (num >> 8) & 0xff;
		data[cur++] = (num >> 16) & 0xff;
		data[cur++] = (num >> 24) & 0xff;
	}
	
	/**
	 * �o�b�t�@�̏����o��
	 * @param buf �o�b�t�@
	 * @param size �o�̓o�C�g��
	 */
	void writeBuffer(const char *buf, int size) {
		resize(cur + size);
		memcpy((void*)&data[cur], buf, size);
		cur += size;
	}

	/**
	 * �摜���̏����o��
	 * @param width �摜����
	 * @param height �摜�c��
	 * @param buffer �摜�o�b�t�@
	 * @param pitch �摜�f�[�^�̃s�b�`
	 */
	bool compress(int width, int height, const unsigned char *buffer, int pitch, ProgressFunc *progress=NULL, void *progressData = NULL) {

		bool canceled = false;
		
		// ARGB �Œ�
		int colors = 4;
	
		// header
		{
			writeBuffer("TLG5.0\x00raw\x1a\x00", 11);
			writeInt8(colors);
			writeInt32(width);
			writeInt32(height);
			writeInt32(BLOCK_HEIGHT);
		}

		int blockcount = (int)((height - 1) / BLOCK_HEIGHT) + 1;

		// buffers/compressors
		SlideCompressor * compressor = NULL;
		unsigned char *cmpinbuf[4];
		unsigned char *cmpoutbuf[4];
		for (int i = 0; i < colors; i++) {
			cmpinbuf[i] = cmpoutbuf[i] = NULL;
		}
		long written[4];
		int *blocksizes;
		
		// allocate buffers/compressors
		try	{
			compressor = new SlideCompressor();
			for(int i = 0; i < colors; i++)	{
				cmpinbuf[i] = new unsigned char [width * BLOCK_HEIGHT];
				cmpoutbuf[i] = new unsigned char [width * BLOCK_HEIGHT * 9 / 4];
				written[i] = 0;
			}
			blocksizes = new int[blockcount];

			// �u���b�N�T�C�Y�̈ʒu���L�^
			ULONG blocksizepos = cur;

			cur += blockcount * 4;

			//
			int block = 0;
			for(int blk_y = 0; blk_y < height; blk_y += BLOCK_HEIGHT, block++) {
				if (progress) {
					if (progress(blk_y * 100 / height, progressData)) {
						canceled = true;
						break;
					}
				}
				int ylim = blk_y + BLOCK_HEIGHT;
				if(ylim > height) ylim = height;
				
				int inp = 0;
				
				for(int y = blk_y; y < ylim; y++) {
					// retrieve scan lines
					const unsigned char * upper;
					if (y != 0) {
						upper = (const unsigned char *)buffer;
						buffer += pitch;
					} else { 
						upper = NULL;
					}
					const unsigned char * current;
					current = (const unsigned char *)buffer;
					
					// prepare buffer
					int prevcl[4];
					int val[4];
					
					for(int c = 0; c < colors; c++) prevcl[c] = 0;
					
					for(int x = 0; x < width; x++) {
						for(int c = 0; c < colors; c++) {
							int cl;
							if(upper)
								cl = 0[current++] - 0[upper++];
							else
								cl = 0[current++];
							val[c] = cl - prevcl[c];
							prevcl[c] = cl;
						}
						// composite colors
						switch(colors){
						case 1:
							cmpinbuf[0][inp] = val[0];
							break;
						case 3:
							cmpinbuf[0][inp] = val[0] - val[1];
							cmpinbuf[1][inp] = val[1];
							cmpinbuf[2][inp] = val[2] - val[1];
							break;
						case 4:
							cmpinbuf[0][inp] = val[0] - val[1];
							cmpinbuf[1][inp] = val[1];
							cmpinbuf[2][inp] = val[2] - val[1];
							cmpinbuf[3][inp] = val[3];
							break;
						}
						inp++;
					}
				}
				
				// compress buffer and write to the file
				
				// LZSS
				int blocksize = 0;
				for(int c = 0; c < colors; c++) {
					long wrote = 0;
					compressor->Store();
					compressor->Encode(cmpinbuf[c], inp,
									   cmpoutbuf[c], wrote);
					if(wrote < inp)	{
						writeInt8(0x00);
						writeInt32(wrote);
						writeBuffer((const char *)cmpoutbuf[c], wrote);
						blocksize += wrote + 4 + 1;
					} else {
						compressor->Restore();
						writeInt8(0x01);
						writeInt32(inp);
						writeBuffer((const char *)cmpinbuf[c], inp);
						blocksize += inp + 4 + 1;
					}
					written[c] += wrote;
				}
			
				blocksizes[block] = blocksize;
			}

			if (!canceled) {
				// �u���b�N�T�C�Y�i�[
				for (int i = 0; i < blockcount; i++) {
					writeInt32(blocksizes[i], blocksizepos);
					blocksizepos += 4;
				}
			}

		} catch(...) {
			for(int i = 0; i < colors; i++) {
				if(cmpinbuf[i]) delete [] cmpinbuf[i];
				if(cmpoutbuf[i]) delete [] cmpoutbuf[i];
			}
			if(compressor) delete compressor;
			if(blocksizes) delete [] blocksizes;
			throw;
		}
		for(int i = 0; i < colors; i++) {
			if(cmpinbuf[i]) delete [] cmpinbuf[i];
			if(cmpoutbuf[i]) delete [] cmpoutbuf[i];
		}
		if(compressor) delete compressor;
		if(blocksizes) delete [] blocksizes;

		if (progress) {
			progress(100, progressData);
		}
		
		return canceled;
	}

	/**
	 * �^�O�W�J�p
	 */
	class TagsCaller : public tTJSDispatch /** EnumMembers �p */ {
	protected:
		ttstr *store;
	public:
		TagsCaller(ttstr *store) : store(store) {};
		virtual tjs_error TJS_INTF_METHOD FuncCall( // function invocation
													tjs_uint32 flag,			// calling flag
													const tjs_char * membername,// member name ( NULL for a default member )
													tjs_uint32 *hint,			// hint for the member name (in/out)
													tTJSVariant *result,		// result
													tjs_int numparams,			// number of parameters
													tTJSVariant **param,		// parameters
													iTJSDispatch2 *objthis		// object as "this"
													) {
			if (numparams > 1) {
				if ((int)param[1] != TJS_HIDDENMEMBER) {
					ttstr name  = *param[0];
					ttstr value = *param[2];
					*store += ttstr(name.GetNarrowStrLen()) + ":" + name + "=" +	ttstr(value.GetNarrowStrLen()) + ":" + value + ",";
				}
			}
			if (result) {
				*result = true;
			}
			return TJS_S_OK;
		}
	};

	
	/**
	 * �摜���̏����o��
	 * @param width �摜����
	 * @param height �摜�c��
	 * @param buffer �摜�o�b�t�@
	 * @param pitch �摜�f�[�^�̃s�b�`
	 * @param tagsDict �^�O���
	 * @return �L�����Z�����ꂽ�� true
	 */
	bool compress(int width, int height, const unsigned char *buffer, int pitch, iTJSDispatch2 *tagsDict, ProgressFunc *progress=NULL, void *progressData=NULL) {

		bool canceled = false;
		
		// �擾
		ttstr tags;
		if (tagsDict) {
			TagsCaller caller(&tags);
			tTJSVariantClosure closure(&caller);
			tagsDict->EnumMembers(TJS_IGNOREPROP, &closure, tagsDict);
		}
		
		ULONG tagslen = tags.GetNarrowStrLen(); 
		if (tagslen > 0) {
			// write TLG0.0 Structured Data Stream header
			writeBuffer("TLG0.0\x00sds\x1a\x00", 11);
			ULONG rawlenpos = cur;
			cur += 4;
			// write raw TLG stream
			if (!(canceled = compress(width, height, buffer, pitch, progress, progressData))) {
				// write raw data size
				writeInt32(cur - rawlenpos - 4, rawlenpos);
				// write "tags" chunk name
				writeBuffer("tags", 4);
				// write chunk size
				writeInt32(tagslen);
				// write chunk data
				resize(cur + tagslen);
				tags.ToNarrowStr((tjs_nchar*)&data[cur], tagslen);
				cur += tagslen;
			}
		} else {
			// write raw TLG stream
			canceled = compress(width, height, buffer, pitch, progress, progressData);
		}
		return canceled;
	}

	/**
	 * �f�[�^���t�@�C���ɏ����o��
	 * @param out �o�͐�X�g���[��
	 */
	void store(IStream *out) {
		ULONG s;
		out->Write(&data[0], size, &s);
	}
};


//---------------------------------------------------------------------------

/**
 * ���C���̓��e�� TLG5�ŕۑ�����
 * @param layer ���C��
 * @param filename �t�@�C����
 * @return �L�����Z�����ꂽ�� true
 */
static bool
saveLayerImageTlg5(iTJSDispatch2 *layer, const tjs_char *filename, iTJSDispatch2 *info, ProgressFunc *progress=NULL, void *progressData=NULL)
{
	// ���C���摜���
	tjs_int width, height, pitch;
	unsigned char* buffer;
	{
		tTJSVariant var;
		layer->PropGet(0, L"imageWidth", NULL, &var, layer);
		width = (tjs_int)var;
		layer->PropGet(0, L"imageHeight", NULL, &var, layer);
		height = (tjs_int)var;
		layer->PropGet(0, L"mainImageBufferPitch", NULL, &var, layer);
		pitch = (tjs_int)var;
		layer->PropGet(0, L"mainImageBuffer", NULL, &var, layer);
		buffer = (unsigned char*)(tjs_int)var;
	}

	// ���k�������s
	Compress compress;
	bool canceled = compress.compress(width, height, buffer, pitch, info, progress, progressData);

	// ���k���L�����Z������Ă��Ȃ���΃t�@�C���ۑ�
	if (!canceled) {
		IStream *out = TVPCreateIStream(filename, TJS_BS_WRITE);
		if (!out) {
			ttstr msg = filename;
			msg += L":can't open";
			TVPThrowExceptionMessage(msg.c_str());
		}
		try {
			// �i�[
			compress.store(out);
		} catch (...) {
			out->Release();
			throw;
		}
		out->Release();
	}
	
	return canceled;
}

//---------------------------------------------------------------------------
// �E�C���h�E�g��
//---------------------------------------------------------------------------

class WindowSaveImage;

/**
 * �Z�[�u�����X���b�h�p���
 */
class SaveInfo {

	friend class WindowSaveImage;
	
protected:

	// �������ϐ�
	WindowSaveImage *notify; //< ���ʒm��
	tTJSVariant layer; //< ���C��
	tTJSVariant filename; //< �t�@�C����
	tTJSVariant info;  //< �ۑ��p�^�O���
	bool canceled;        //< �L�����Z���w��
	tTJSVariant handler;  //< �n���h���l
	tTJSVariant progressPercent; //< �i�s�x����
	
protected:
	/**
	 * ���݂̏�Ԃ̒ʒm
	 * @param percent �p�[�Z���g
	 */
	bool progress(int percent);

	/**
	 * �Ăяo���p
	 */
	static bool progressFunc(int percent, void *userData) {
		SaveInfo *self = (SaveInfo*)userData;
		return self->progress(percent);
	}
	
	// �o�߃C�x���g���M
	void eventProgress(iTJSDispatch2 *objthis) {
		tTJSVariant *vars[] = {&handler, &progressPercent, &layer, &filename};
		objthis->FuncCall(0, L"onSaveLayerImageProgress", NULL, NULL, 4, vars, objthis);
	}

	// �I���C�x���g���M
	void eventDone(iTJSDispatch2 *objthis) {
		tTJSVariant result = canceled ? 1 : 0;
		tTJSVariant *vars[] = {&handler, &result, &layer, &filename};
		objthis->FuncCall(0, L"onSaveLayerImageDone", NULL, NULL, 4, vars, objthis);
	}
	
public:
	// �R���X�g���N�^
	SaveInfo(int handler, WindowSaveImage *notify, tTJSVariant layer, const tjs_char *filename, tTJSVariant info)
		: handler(handler), notify(notify), layer(layer), filename(filename), canceled(false) {}
	
	// �f�X�g���N�^
	~SaveInfo() {}

	// �n���h���擾
	int getHandler() {
		return (int)handler;
	}
	
 	// �����J�n
	void start();

	// �����L�����Z��
	void cancel() {
		canceled = true;
	}

	// �����I��
	void stop() {
		canceled = true;
		notify = NULL;
	}
};

/**
 * �E�C���h�E�Ƀ��C���Z�[�u�@�\���g��
 */
class WindowSaveImage {

protected:
	iTJSDispatch2 *objthis; //< �I�u�W�F�N�g���̎Q��

	vector<SaveInfo*> saveinfos; //< �Z�[�u�����ێ��p

	// ���s�X���b�h
	static void checkThread(void *data) {
		((SaveInfo*)data)->start();
	}

	// �o�ߒʒm
	void eventProgress(SaveInfo *sender) {
		int handler = sender->getHandler();
		if (saveinfos[handler] == sender) {
			sender->eventProgress(objthis);
		}
	}

	// �I���ʒm
	void eventDone(SaveInfo *sender) {
		int handler = sender->getHandler();
		if (saveinfos[handler] == sender) {
			saveinfos[handler] = NULL;
			sender->eventDone(objthis);
		}
		delete sender;
	}

	/*
	 * �E�C���h�E�C�x���g�������V�[�o
	 */
	static bool __stdcall receiver(void *userdata, tTVPWindowMessage *Message) {
		if (Message->Msg == WM_SAVE_TLG_PROGRESS) {
			iTJSDispatch2 *obj = (iTJSDispatch2*)userdata;
			WindowSaveImage *self = ncbInstanceAdaptor<WindowSaveImage>::GetNativeInstance(obj);
			if (self) {
				self->eventProgress((SaveInfo*)Message->WParam);
			}
			return true;
		} else if (Message->Msg == WM_SAVE_TLG_DONE) {
			iTJSDispatch2 *obj = (iTJSDispatch2*)userdata;
			WindowSaveImage *self = ncbInstanceAdaptor<WindowSaveImage>::GetNativeInstance(obj);
			if (self) {
				self->eventDone((SaveInfo*)Message->WParam);
			}
			return true;
		}
		return false;
	}

	// ���[�U���b�Z�[�W���V�[�o�̓o�^/����
	void setReceiver(tTVPWindowMessageReceiver receiver, bool enable) {
		tTJSVariant mode     = enable ? (tTVInteger)(tjs_int)wrmRegister : (tTVInteger)(tjs_int)wrmUnregister;
		tTJSVariant proc     = (tTVInteger)(tjs_int)receiver;
		tTJSVariant userdata = (tTVInteger)(tjs_int)objthis;
		tTJSVariant *p[] = {&mode, &proc, &userdata};
		if (objthis->FuncCall(0, L"registerMessageReceiver", NULL, NULL, 4, p, objthis) != TJS_S_OK) {
			TVPThrowExceptionMessage(L"can't regist user message receiver");
		}
	}

public:

	/**
	 * �R���X�g���N�^
	 */
	WindowSaveImage(iTJSDispatch2 *objthis) : objthis(objthis) {
		setReceiver(receiver, true);
	}

	/**
	 * �f�X�g���N�^
	 */
	~WindowSaveImage() {
		setReceiver(receiver, false);
		for (int i=0;i<(int)saveinfos.size();i++) {
			SaveInfo *saveinfo = saveinfos[i];
			if (saveinfo) {
				saveinfo->stop();
				saveinfos[i] = NULL;
			}
		}
	}

	/**
	 * ���b�Z�[�W���M
	 * @param msg ���b�Z�[�W
	 * @param wparam WPARAM
	 * @param lparam LPARAM
	 */
	void postMessage(UINT msg, WPARAM wparam=NULL, LPARAM lparam=NULL) {
		// �E�B���h�E�n���h�����擾���Ēʒm
		tTJSVariant val;
		objthis->PropGet(0, TJS_W("HWND"), NULL, &val, objthis);
		HWND hwnd = reinterpret_cast<HWND>((tjs_int)(val));
		::PostMessage(hwnd, msg, wparam, lparam);
	}

	/**
	 * ���C���Z�[�u�J�n
	 * @param layer ���C��
	 * @param filename �t�@�C����
	 * @param info �^�O���
	 */
	int startSaveLayerImage(tTJSVariant layer, const tjs_char *filename, tTJSVariant info) {
		int handler = saveinfos.size();
		for (int i=0;i<(int)saveinfos.size();i++) {
			if (saveinfos[i] == NULL) {
				handler = i;
				break;
			}
		}
		if (handler >= (int)saveinfos.size()) {
			saveinfos.resize(handler + 1);
		}

		// �ۑ��p�Ƀ��C���𕡐�����
		tTJSVariant newLayer;
		{
			// �V�������C���𐶐�
			tTJSVariant window(objthis, objthis);
			tTJSVariant primaryLayer;
			objthis->PropGet(0, L"primaryLayer", NULL, &primaryLayer, objthis);
			tTJSVariant *vars[] = {&window, &primaryLayer};
			iTJSDispatch2 *obj;
			if (TJS_SUCCEEDED(layerClass->CreateNew(0, NULL, NULL, &obj, 2, vars, objthis))) {

				// ���O�Â�
				tTJSVariant name = "saveLayer:";
				name +=filename;
				obj->PropSet(0, L"name", NULL, &name, obj);

				// �����C���̉摜�𕡐�
				tTJSVariant *param[] = {&layer};
				if (TJS_SUCCEEDED(layerAssignImages->FuncCall(0, NULL, NULL, NULL, 1, param, obj))) {
					newLayer = tTJSVariant(obj, obj);
					obj->Release();
				} else {
					obj->Release();
					TVPThrowExceptionMessage(L"�ۑ������p���C���ւ̉摜�̕����Ɏ��s���܂���");
				}
			} else {
				TVPThrowExceptionMessage(L"�ۑ������p���C���̐����Ɏ��s���܂���");
			}
		}
		SaveInfo *saveInfo = new SaveInfo(handler, this, newLayer, filename, info);
		saveinfos[handler] = saveInfo;
		_beginthread(checkThread, 0, saveInfo);
		return handler;
	}
	
	/**
	 * ���C���Z�[�u�̃L�����Z��
	 */
	void cancelSaveLayerImage(int handler) {
		if (handler < (int)saveinfos.size() && saveinfos[handler] != NULL) {
			saveinfos[handler]->cancel();
		}
	}

	/**
	 * ���C���Z�[�u�̒��~
	 */
	void stopSaveLayerImage(int handler) {
		if (handler < (int)saveinfos.size() && saveinfos[handler] != NULL) {
			saveinfos[handler]->stop();
			saveinfos[handler] = NULL;
		}
	}
};


/**
 * ���݂̏�Ԃ̒ʒm
 * @param percent �p�[�Z���g
 */
bool
SaveInfo::progress(int percent)
{
	if ((int)progressPercent != percent) {
		progressPercent = percent;
		if (notify) {
			notify->postMessage(WM_SAVE_TLG_PROGRESS, (WPARAM)this);
			Sleep(0);
		}
	}
	return canceled;
}

/*
 * �ۑ������J�n
 */
void
SaveInfo::start()
{
	// �摜���Z�[�u
	saveLayerImageTlg5(layer.AsObjectNoAddRef(), // layer
					   filename.GetString(),
					   info.Type() == tvtObject ? info.AsObjectNoAddRef() : NULL, // info
					   progressFunc, this);
	// �����ʒm
	if (notify) {
		notify->postMessage(WM_SAVE_TLG_DONE, (WPARAM)this);
		Sleep(0);
	} else {
		delete this;
	}
}

//---------------------------------------------------------------------------

// �C���X�^���X�Q�b�^
NCB_GET_INSTANCE_HOOK(WindowSaveImage)
{
	NCB_INSTANCE_GETTER(objthis) { // objthis �� iTJSDispatch2* �^�̈����Ƃ���
		ClassT* obj = GetNativeInstance(objthis);	// �l�C�e�B�u�C���X�^���X�|�C���^�擾
		if (!obj) {
			obj = new ClassT(objthis);				// �Ȃ��ꍇ�͐�������
			SetNativeInstance(objthis, obj);		// objthis �� obj ���l�C�e�B�u�C���X�^���X�Ƃ��ēo�^����
		}
		return obj;
	}
};

NCB_ATTACH_CLASS_WITH_HOOK(WindowSaveImage, Window) {
	NCB_METHOD(startSaveLayerImage);
	NCB_METHOD(cancelSaveLayerImage);
	NCB_METHOD(stopSaveLayerImage);
};

//---------------------------------------------------------------------------
// ���C���g��
//---------------------------------------------------------------------------

// �o�b�t�@�Q�Ɨp�̌^
typedef unsigned char const *BufRefT;

/**
 * ���C���̃T�C�Y�ƃo�b�t�@���擾����
 */
static bool
GetLayerBufferAndSize(iTJSDispatch2 *lay, long &w, long &h, BufRefT &ptr, long &pitch)
{
	// ���C���C���X�^���X�ȊO�ł̓G���[
	if (!lay || TJS_FAILED(lay->IsInstanceOf(0, 0, 0, TJS_W("Layer"), lay))) return false;

	// ���C���C���[�W�͍݂邩�H
	tTJSVariant val;
	if (TJS_FAILED(lay->PropGet(0, TJS_W("hasImage"), 0, &val, lay)) || (val.AsInteger() == 0)) return false;

	// ���C���T�C�Y���擾
	val.Clear();
	if (TJS_FAILED(lay->PropGet(0, TJS_W("imageWidth"), 0, &val, lay))) return false;
	w = (long)val.AsInteger();

	val.Clear();
	if (TJS_FAILED(lay->PropGet(0, TJS_W("imageHeight"), 0, &val, lay))) return false;
	h = (long)val.AsInteger();

	// �T�C�Y���s��
	if (w <= 0 || h <= 0) return false;

	// �o�b�t�@�擾
	val.Clear();
	if (TJS_FAILED(lay->PropGet(0, TJS_W("mainImageBuffer"),      0, &val, lay))) return false;
	ptr = reinterpret_cast<BufRefT>(val.AsInteger());

	// �s�b�`�擾
	val.Clear();
	if (TJS_FAILED(lay->PropGet(0, TJS_W("mainImageBufferPitch"), 0, &val, lay))) return false;
	pitch = (long)val.AsInteger();

	// �擾���s
	if (ptr == 0 || pitch == 0) return false;

	return true;
}

/**
 * ��`�̈�̎����𐶐�
 */
static void
MakeResult(tTJSVariant *result, long x, long y, long w, long h)
{
	ncbDictionaryAccessor dict;
	dict.SetValue(TJS_W("x"), x);
	dict.SetValue(TJS_W("y"), y);
	dict.SetValue(TJS_W("w"), w);
	dict.SetValue(TJS_W("h"), h);
	*result = dict;
}

/**
 * �s�����`�F�b�N�֐�
 */
static bool
CheckTransp(BufRefT p, long next, long count)
{
	for (; count > 0; count--, p+=next) if (p[3] != 0) return true;
	return false;
}

/**
 * ���C���C���[�W���N���b�v�i�㉺���E�̗]������������؂���j�����Ƃ��̃T�C�Y���擾����
 *
 * Layer.getCropRect = function();
 * @return %[ x, y, w, h] �`���̎����C�܂���void�i�S�������̂Ƃ��j
 */
static tjs_error TJS_INTF_METHOD
GetCropRect(tTJSVariant *result, tjs_int numparams, tTJSVariant **param, iTJSDispatch2 *lay)
{
	// ���C���o�b�t�@�̎擾
	BufRefT p, r = 0;
	long w, h, nl, nc = 4;
	if (!GetLayerBufferAndSize(lay, w, h, r, nl))
		TVPThrowExceptionMessage(TJS_W("Invalid layer image."));

	// ���ʗ̈�
	long x1=0, y1=0, x2=w-1, y2=h-1;
	result->Clear();

	for (p=r;             x1 <  w; x1++,p+=nc) if (CheckTransp(p, nl,  h)) break; // �����瓧���̈�𒲂ׂ�
	/*                                      */ if (x1 >= w) return TJS_S_OK;      // �S�������Ȃ� void ��Ԃ�
	for (p=r+x2*nc;       x2 >= 0; x2--,p-=nc) if (CheckTransp(p, nl,  h)) break; // �E���瓧���̈�𒲂ׂ�
	/*                                      */ long rw = x2 - x1 + 1;             // ���E�ɋ��܂ꂽ�c��̕�
	for (p=r+x1*nc;       y1 <  h; y1++,p+=nl) if (CheckTransp(p, nc, rw)) break; // �ォ�瓧���̈�𒲂ׂ�
	for (p=r+x1*nc+y2*nl; y2 >= 0; y2--,p-=nl) if (CheckTransp(p, nc, rw)) break; // �����瓧���̈�𒲂ׂ�

	// ���ʂ������ɕԂ�
	MakeResult(result, x1, y1, rw, y2 - y1 + 1);

	return TJS_S_OK;
}

NCB_ATTACH_FUNCTION(getCropRect, Layer, GetCropRect);

/**
 * �F��r�֐�
 */
static bool
CheckDiff(BufRefT p1, long p1n, BufRefT p2, long p2n, long count)
{
	for (; count > 0; count--, p1+=p1n, p2+=p2n)
		if ((p1[3] != p2[3]) || (p1[3] != 0 && (p1[0] != p2[0] || p1[1] != p2[1] || p1[2] != p2[2]))) return true;
	// ������0�̏ꍇ�͐F���ǂ�Ȃ��̂ł������Ƃ݂Ȃ�
	return false;
}

/**
 * ���C���̍����̈���擾����i
 * 
 * Layer.getDiffRegion = function(base);
 * @param base �������ƂȂ�x�[�X�p�̉摜�i�C���X�^���X���g�Ɠ����摜�T�C�Y�ł��邱�Ɓj
 * @return %[ x, y, w, h ] �`���̎����C�܂���void�i���S�ɓ����摜�̂Ƃ��j
 */

static tjs_error TJS_INTF_METHOD
GetDiffRect(tTJSVariant *result, tjs_int numparams, tTJSVariant **param, iTJSDispatch2 *lay)
{
	// �����̐��`�F�b�N
	if (numparams < 1) return TJS_E_BADPARAMCOUNT;

	iTJSDispatch2 *base = param[0]->AsObjectNoAddRef();

	// ���C���o�b�t�@�̎擾
	BufRefT fp, tp, fr = 0, tr = 0;
	long w, h, tnl, fw, fh, fnl, nc = 4;
	if (!GetLayerBufferAndSize(lay,   w,  h, tr, tnl) || 
		!GetLayerBufferAndSize(base, fw, fh, fr, fnl))
		TVPThrowExceptionMessage(TJS_W("Invalid layer image."));

	// ���C���̃T�C�Y�͓�����
	if (w != fw || h != fh)
		TVPThrowExceptionMessage(TJS_W("Different layer size."));

	// ���ʗ̈�
	long x1=0, y1=0, x2=w-1, y2=h-1;
	result->Clear();

	for (fp=fr,             tp=tr;              x1 <  w; x1++,fp+=nc, tp+=nc ) if (CheckDiff(fp, fnl, tp, tnl,  h)) break; // �����瓧���̈�𒲂ׂ�
	/*                                                                      */ if (x1 >= w) return TJS_S_OK;               // �S�������Ȃ� void ��Ԃ�
	for (fp=fr+x2*nc,       tp=tr+x2*nc;        x2 >= 0; x2--,fp-=nc, tp-=nc ) if (CheckDiff(fp, fnl, tp, tnl,  h)) break; // �E���瓧���̈�𒲂ׂ�
	/*                                                                      */ long rw = x2 - x1 + 1;                      // ���E�ɋ��܂ꂽ�c��̕�
	for (fp=fr+x1*nc,       tp=tr+x1*nc;        y1 <  h; y1++,fp+=fnl,tp+=tnl) if (CheckDiff(fp, nc,  tp, nc,  rw)) break; // �ォ�瓧���̈�𒲂ׂ�
	for (fp=fr+x1*nc+y2*fnl,tp=tr+x1*nc+y2*tnl; y2 >= 0; y2--,fp-=fnl,tp-=tnl) if (CheckDiff(fp, nc,  tp, nc,  rw)) break; // �����瓧���̈�𒲂ׂ�

	// ���ʂ������ɕԂ�
	MakeResult(result, x1, y1, rw, y2 - y1 + 1);

	return TJS_S_OK;
}

NCB_ATTACH_FUNCTION(getDiffRect, Layer, GetDiffRect);

/**
 * TLG�摜�ۑ�
 */
static tjs_error TJS_INTF_METHOD saveLayerImageTlg5Func(tTJSVariant *result,
														tjs_int numparams,
														tTJSVariant **param,
														iTJSDispatch2 *objthis) {
	if (numparams < 1) return TJS_E_BADPARAMCOUNT;
	saveLayerImageTlg5(objthis, // layer
					   param[0]->GetString(),  // filename
					   numparams > 1 ? param[1]->AsObjectNoAddRef() : NULL // info
					   );
	return TJS_S_OK;
};

NCB_ATTACH_FUNCTION(saveLayerImageTlg5, Layer, saveLayerImageTlg5Func);

/**
 * �o�^������
 */
static void PostRegistCallback()
{
	tTJSVariant var;
	TVPExecuteExpression(TJS_W("Layer"), &var);
	layerClass = var.AsObject();
	TVPExecuteExpression(TJS_W("Layer.assignImages"), &var);
	layerAssignImages = var.AsObject();
}

#define RELEASE(name) name->Release();name= NULL

/**
 * �J�������O
 */
static void PreUnregistCallback()
{
	RELEASE(layerClass);
	RELEASE(layerAssignImages);
}

NCB_POST_REGIST_CALLBACK(PostRegistCallback);
NCB_PRE_UNREGIST_CALLBACK(PreUnregistCallback);
