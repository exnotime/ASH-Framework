
class Tile{
    Vec2 textureIndex;
    bool filled;
}

class Level {
    Level(){

    }
    Level(Vec2 size){
        m_Size = size;
        m_Grid.resize(size.x, size.y);
        m_TileSize = Vec2(64, 64);
        Clear();
    }

    void Clear() {
        for ( uint y = 0; y < m_Grid.height(); ++y ) {
            for ( uint x = 0; x < m_Grid.width(); ++x ) {
                m_Grid[x,y].textureIndex = 1;
                m_Grid[x,y].filled = false;
            }
        }
    }

    void reloadLevelGrid() {
        Clear();
    }

    void SelectTile(int x, int y){
        const array<int> values = {1,2,4,8,16,32,64,128,256,512};
        int val = 0;
        for(int i = -1; i < 2; ++i){
            for(int k = -1; k < 2; ++k){
                if(x + i < 0 || x + i >= m_Grid.width() || y + k < 0 || y + k >= m_Grid.height()){
                    continue;
                }
                if(m_Grid[x + i, y + k].filled){
                    val += values[(k + 1) * 3 + (i + 1)];
                }
            }
        }
        //walls
        if(val == 219){
            m_Grid[x,y].textureIndex = Vec2(2, 11);
        }else if (val == 223){
            m_Grid[x,y].textureIndex = Vec2(2, 11);
        }else if (val == 438){
            m_Grid[x,y].textureIndex = Vec2(0, 11);
        }else if (val == 439){
            m_Grid[x,y].textureIndex = Vec2(0, 11);
        }else if (val == 63){
            m_Grid[x,y].textureIndex = Vec2(1, 12);
        }else if (val == 127){
            m_Grid[x,y].textureIndex = Vec2(1, 12);
        }else if (val == 504){
            m_Grid[x,y].textureIndex = Vec2(1, 10);
        }else if (val == 505){
            m_Grid[x,y].textureIndex = Vec2(1, 10);
        }else if (val == 216){ //corners 
            m_Grid[x,y].textureIndex = Vec2(2, 10);
        }else if (val == 27){ 
            m_Grid[x,y].textureIndex = Vec2(2, 12);
        }else if (val == 54){ 
            m_Grid[x,y].textureIndex = Vec2(0, 12);
        }else if (val == 432){ 
            m_Grid[x,y].textureIndex = Vec2(0, 10);
        }else if (val == 510){ 
            m_Grid[x,y].textureIndex = Vec2(4, 11);
        }else if (val == 507){ 
            m_Grid[x,y].textureIndex = Vec2(3, 11);
        }else if (val == 447){ 
            m_Grid[x,y].textureIndex = Vec2(4, 11);
        }else if (val == 255){ 
            m_Grid[x,y].textureIndex = Vec2(3, 10);
        }else if(m_Grid[x,y].filled){
            m_Grid[x,y].textureIndex = Vec2(1, 11);
        }else{
            m_Grid[x,y].textureIndex = Vec2(9, 7);
        }
    }

    void Init() {
        m_Textures.insertLast(LoadTexture("assets/textures/roguelikeDungeon_transparent.png"));

        @m_Font = LoadFont("assets/fonts/jackinput.ttf");
        @m_BackgroundLayer = CreateRenderTexture("LevelBackground", m_Size * m_TileSize);
        m_Grid.resize(m_Size.x, m_Size.y);
        for(uint i =  0; i < 2; ++i){
            int startX = Random(0, m_Grid.width() - 2);
            int startY = Random(0, m_Grid.height() - 2);
            int endX = Random(startX + 2, m_Grid.width());
            int endY = Random(startY + 2, m_Grid.height());

            for ( int y = startY; y < endY; ++y ) {
                for ( int x = startX; x < endX; ++x ) {
                    m_Grid[x,y].filled = true;
                }
            }
        }
        for ( int y = 0; y < m_Grid.height(); ++y ) {
            for ( int x = 0; x < m_Grid.width(); ++x ) {
                SelectTile(x,y);
            }
        }
        ClearRenderTexture(m_BackgroundLayer, Color(0,0,0));
        for (int y = 0; y < m_Grid.height(); ++y ) {
            for ( int x = 0; x < m_Grid.width(); ++x ) {
                DrawSpriteToTexture(m_BackgroundLayer, m_Textures[0], Vec2(x,y) * m_TileSize, Vec2(4), m_Grid[x,y].textureIndex * Vec2(16) + m_Grid[x,y].textureIndex, Vec2(16));
            }
        }
        for (int y = 0; y < m_Grid.height(); ++y ) {
            DrawRectToTexture(m_BackgroundLayer, Vec2(0, y * m_TileSize.y), Vec2(m_Grid.width() * m_TileSize.x, 1),Color(255,0,0));
        }
        for (int x = 0; x < m_Grid.width(); ++x ) {
            DrawRectToTexture(m_BackgroundLayer, Vec2(x * m_TileSize.x, 0), Vec2(1, m_Grid.height() * m_TileSize.y),Color(255,0,0));
        }
        FlushRenderTexture(m_BackgroundLayer);
    }

    void Render(float fps){
        DrawText(m_Font, "Fps: " + fps, Vec2(0), Color(0,0,0));
        DrawText(m_Font, "Width: " + m_Grid.width() + " Height: " + m_Grid.height(), Vec2(0, 30), Color(0,0,0));
        DrawSprite(GetTextureFromRenderTexture(m_BackgroundLayer), Vec2(0), Vec2(1), 1);
    }

    bool IsWalkable(Vec2 coord){
        return m_Grid[coord.x, coord.y].filled == false;
    }

    Font@ m_Font;
    RenderTexture@ m_BackgroundLayer;
    Vec2 m_TileSize;
    Vec2 m_Size;
    array<Texture@> m_Textures;
    grid<Tile> m_Grid;
};
