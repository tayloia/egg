namespace egg0
{
  class ILexer
  {

  };

  namespace LexerFactory
  {
    std::shared_ptr<ILexer> createFromStream(std::istream& stream);
  };
}
