#include "ncbind/ncbind.hpp"
#include <vector>
using namespace std;

#include <tlg5/slide.h>

#define BLOCK_HEIGHT 4
//---------------------------------------------------------------------------

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
	void compress(int width, int height, const unsigned char *buffer, int pitch) {

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

			// �u���b�N�T�C�Y�i�[
			for (int i = 0; i < blockcount; i++) {
				writeInt32(blocksizes[i], blocksizepos);
				blocksizepos += 4;
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
	 */
	void compress(int width, int height, const unsigned char *buffer, int pitch, iTJSDispatch2 *tagsDict) {

		// �擾
		ttstr tags;
		TagsCaller caller(&tags);
		tTJSVariantClosure closure(&caller);
		tagsDict->EnumMembers(TJS_IGNOREPROP, &closure, tagsDict);
		
		ULONG tagslen = tags.GetNarrowStrLen(); 
		if (tagslen > 0) {
			// write TLG0.0 Structured Data Stream header
			writeBuffer("TLG0.0\x00sds\x1a\x00", 11);
			ULONG rawlenpos = cur;
			cur += 4;
			// write raw TLG stream
			compress(width, height, buffer, pitch);
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
		} else {
			// write raw TLG stream
			compress(width, height, buffer, pitch);
		}
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

#include "ncbind/ncbind.hpp"

struct layerExSave {
	static tjs_error TJS_INTF_METHOD saveLayerImageTlg5(tTJSVariant *result,
														tjs_int numparams,
														tTJSVariant **param,
														iTJSDispatch2 *objthis) {
		if (numparams < 1) return TJS_E_BADPARAMCOUNT;

		const tjs_char *filename = param[0]->GetString();
		IStream *out = TVPCreateIStream(filename, TJS_BS_WRITE);
		if (!out) {
			ttstr msg = filename;
			msg += L":can't open";
			TVPThrowExceptionMessage(msg.c_str());
		}

		// ���C���摜���
		tjs_int width, height, pitch;
		unsigned char* buffer;
		{
			tTJSVariant var;
			objthis->PropGet(0, L"imageWidth", NULL, &var, objthis);
			width = (tjs_int)var;
			objthis->PropGet(0, L"imageHeight", NULL, &var, objthis);
			height = (tjs_int)var;
			objthis->PropGet(0, L"mainImageBufferPitch", NULL, &var, objthis);
			pitch = (tjs_int)var;
			objthis->PropGet(0, L"mainImageBuffer", NULL, &var, objthis);
			buffer = (unsigned char*)(tjs_int)var;
		}

		// ���k�������s
		try {
			Compress compress;
			if (numparams > 1) {
				// �^�O����
				compress.compress(width, height, buffer, pitch, *param[1]);
			} else {
				compress.compress(width, height, buffer, pitch);
			}
			// �i�[
			compress.store(out);
		} catch (...) {
			out->Release();
			throw;
		}
		out->Release();
		return TJS_S_OK;		
	}



};

NCB_ATTACH_CLASS(layerExSave, Layer) {
	RawCallback("saveLayerImageTlg5", &layerExSave::saveLayerImageTlg5, 0);
}
