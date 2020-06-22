//template<typename P, typename CODER>
//template<typename P2, typename CODER2>
//Bitmap<P2, CODER2> * Bitmap<P, CODER>::convertToBitmap() {
//    auto * bmp_2 = new Bitmap<P2, CODER2>(_width, _height);
//    copyToBitmap(*bmp_2);
//    return bmp_2;
//}
//
//template<typename P, typename CODER>
//template<typename P2, typename CODER2>
//void Bitmap<P, CODER>::copyToBitmap(Bitmap<P2, CODER2> & bmp) {
//    if(bmp.size()!=this->size()) return;
//    const int size = this->size();
//    microgl::color::color_t color_bmp_1, color_bmp_2;
//    for (int index = 0; index < size; ++index) {
//        this->decode(index, color_bmp_1);
//        coder().convert(color_bmp_1, color_bmp_2, bmp.coder());
//        bmp.writeColor(index, color_bmp_2);
//    }
//}
//
////template<typename P, typename CODER>
////Bitmap<P, CODER>::Bitmap(P* $pixels, int w, int h) :
////        FrameBuffer<P>($pixels, w * h), _width{w}, _height{h} {
////}
//
////template<typename P, typename CODER>
////Bitmap<P, CODER>::Bitmap(int w, int h) :
////        Bitmap<P, CODER>::Bitmap(new P[w * h], w, h) {
////}
//
//template<typename P, typename CODER>
//Bitmap<P, CODER>::Bitmap(int w, int h) :
//        Bitmap<P, CODER>::Bitmap(new uint8_t[sizeof(P) * w * h], w, h) {
//}
//
//template<typename P, typename CODER>
//Bitmap<P, CODER>::Bitmap(uint8_t *$pixels, int w, int h) :
//                FrameBuffer<P>(reinterpret_cast<P *>($pixels), w * h), _width{w}, _height{h} {
//}
//
//template<typename P, typename CODER>
//Bitmap<P, CODER>::~Bitmap() {
//    _width = 0;
//    _height = 0;
//}
//
//template<typename P, typename CODER>
//int Bitmap<P, CODER>::width() const {
//    return _width;
//}
//
//template<typename P, typename CODER>
//int Bitmap<P, CODER>::height() const {
//    return _height;
//}
//
//template<typename P, typename CODER>
//P Bitmap<P, CODER>::pixelAt(int x, int y) const {
//    return this->_data[y*_width + x];
//}
//
//template<typename P, typename CODER>
//const microgl::coder::PixelCoder<P, CODER> &Bitmap<P, CODER>::coder() const {
//    return _coder;
//}
//
//template<typename P, typename CODER>
//P Bitmap<P, CODER>::pixelAt(int index) const {
//    return this->_data[index];
//}
//
//template<typename P, typename CODER>
//void Bitmap<P, CODER>::decode(int x, int y, microgl::color::color_t &output)  const{
//    _coder.decode(pixelAt(x, y), output);
//}
//
//template<typename P, typename CODER>
//void Bitmap<P, CODER>::decode(int index, microgl::color::color_t &output)  const{
//    _coder.decode(pixelAt(index), output);
//}
//
//template<typename P, typename CODER>
//template <typename number>
//void Bitmap<P, CODER>::decode(int x, int y, microgl::color::intensity<number> &output) const {
//    _coder.decode(pixelAt(x, y), output);
//}
//
//template<typename P, typename CODER>
//template <typename number>
//void Bitmap<P, CODER>::decode(int index, microgl::color::intensity<number> &output) const {
//    _coder.decode(pixelAt(index), output);
//}
//
//template<typename P, typename CODER>
//void Bitmap<P, CODER>::writeColor(int index, const microgl::color::color_t &color) {
//    P output;
//    _coder.encode(color, output);
//    this->_data[index] = output;
//}
//
//template<typename P, typename CODER>
//void Bitmap<P, CODER>::writeColor(int x, int y, const microgl::color::color_t &color) {
//    writeColor(y*_width + x, color);
//}
//
//template<typename P, typename CODER>
//template <typename number>
//void Bitmap<P, CODER>::writeColor(int index, const microgl::color::intensity<number> &color) {
//    P output;
//    _coder.encode(color, output);
//    this->_data[index] = output;
//}
//
//template<typename P, typename CODER>
//template <typename number>
//void Bitmap<P, CODER>::writeColor(int x, int y, const microgl::color::intensity<number> &color) {
//    writeColor(y*_width + x, color);
//}
//
